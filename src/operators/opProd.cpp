#include "operators/opProd.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "operators/flatten.h"
#include "operators/previousValueCache.h"
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OperatorTrates<OpProd>::OperandsSequenceTrigger;

OpProd::OpProd(OpProd&& other)
    : SimpleUnaryOperator<IntView, SequenceView, OpProd>(std::move(other)),
      evaluationComplete(std::move(other.evaluationComplete)) {}
void OpProd::addSingleValue(Int exprValue) {
    if (exprValue == 0) {
        ++numberZeros;
    } else {
        cachedValue *= exprValue;
    }
    value *= exprValue;
    assert((numberZeros > 0 && value == 0) || (value != 0 && numberZeros == 0));
}
void OpProd::removeSingleValue(Int exprValue) {
    if (exprValue == 0) {
        --numberZeros;
        if (numberZeros == 0) {
            value = cachedValue;
        }
    } else {
        value /= exprValue;
        cachedValue /= exprValue;
    }
}

class OperatorTrates<OpProd>::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    Int previousValue;
    Int previousValueWithoutZeros;
    UInt numberZerosInPreviousValue;
    PreviousValueCache<Int> previousValues;
    OpProd* op;
    OperandsSequenceTrigger(OpProd* op) : op(op) {}
    void valueAdded(UInt, const AnyExprRef& exprIn) final {
        previousValues.clear();
        if (!op->evaluationComplete) {
            return;
        }
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        if (expr->isUndefined()) {
            if (op->operand->view().numberUndefined == 1) {
                op->setDefined(false, false);
                visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                              op->triggers, true);
            }
            return;
        }
        op->changeValue([&]() {
            op->addSingleValue(expr->view().value);
            return true;
        });
    }

    void valueRemoved(UInt, const AnyExprRef& exprIn) final {
        previousValues.clear();
        if (!op->evaluationComplete) {
            return;
        }

        const auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        if (expr->isUndefined()) {
            if (op->operand->view().numberUndefined == 0) {
                op->setDefined(true, false);
                visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                              op->triggers, true);
            }
            return;
        }
        op->changeValue([&]() {
            op->removeSingleValue(expr->view().value);
            return true;
        });
    }

    inline void positionsSwapped(UInt, UInt) { previousValues.clear(); }

    ExprRef<IntView>& getMember(UInt index) {
        return op->operand->view().getMembers<IntView>()[index];
    }
    Int getValueCatchUndef(UInt index) {
        auto& member = getMember(index);
        return (member->isUndefined()) ? 1 : member->view().value;
    }

    inline void possibleSubsequenceChange(UInt startIndex,
                                          UInt endIndex) final {
        if (!op->evaluationComplete) {
            return;
        }

        if (endIndex - startIndex == 1) {
            previousValues.store(startIndex, getValueCatchUndef(startIndex));
            return;
        }

        previousValue = 1;
        previousValueWithoutZeros = 1;
        numberZerosInPreviousValue = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            Int operandValue = getValueCatchUndef(i);
            previousValue *= operandValue;
            if (operandValue != 0) {
                previousValueWithoutZeros *= operandValue;
            } else {
                ++numberZerosInPreviousValue;
            }
        }
    }

    inline void handleSingleOperandChange(UInt index) {
        debug_code(assert((op->numberZeros > 0 && op->value == 0) ||
                          (op->value != 0 && op->numberZeros == 0)));
        Int newValue = getValueCatchUndef(index);
        Int oldValue = previousValues.getAndSet(index, newValue);
        op->changeValue([&]() {
            op->removeSingleValue(oldValue);
            op->addSingleValue(newValue);
            return true;
        });
    }

    void handleMultiOperandChange(Int newValue, Int newValueWithoutZeros) {
        // remove old value if necessary
        if (previousValue == 0) {
            if (op->numberZeros == 0) {
                op->cachedValue /= previousValueWithoutZeros;
                op->value = op->cachedValue;
            }
        } else {
            op->value /= previousValue;
            op->cachedValue /= previousValue;
        }
        // add new value
        if (newValue == 0) {
            op->cachedValue *= newValueWithoutZeros;
        } else {
            op->cachedValue *= newValue;
        }
        op->value *= newValue;
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        if (!op->evaluationComplete) {
            return;
        }
        if (endIndex - startIndex == 1) {
            handleSingleOperandChange(startIndex);
            return;
        }
        Int newValue = 1;
        Int newValueWithoutZeros = 1;
        UInt numberZerosInNewValue = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            Int operandValue = getValueCatchUndef(i);
            newValue *= operandValue;
            if (operandValue != 0) {
                newValueWithoutZeros *= operandValue;
            } else {
                ++numberZerosInNewValue;
            }
        }
        op->numberZeros -= numberZerosInPreviousValue;
        op->numberZeros += numberZerosInNewValue;
        if (previousValue == newValue) {
            return;
        }

        if (!op->isDefined()) {
            handleMultiOperandChange(newValue, newValueWithoutZeros);
        } else {
            op->changeValue([&]() {
                handleMultiOperandChange(newValue, newValueWithoutZeros);
                return true;
            });
        }
    }

    void possibleValueChange() final {}
    void valueChanged() final {
        previousValues.clear();

        if (!op->isDefined()) {
            op->reevaluate();
            if (op->isDefined()) {
                visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                              op->triggers, true);
            }
            return;
        }
        bool isDefined = op->changeValue([&]() {
            op->reevaluate();
            return op->isDefined();
        });
        if (!isDefined) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers, true);
        }
    }

    void reattachTrigger() final {
        deleteTrigger(op->operandTrigger);
        auto trigger = make_shared<OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }

    void hasBecomeUndefined() final {
        previousValues.clear();
        op->setDefined(false, true);
    }
    void hasBecomeDefined() final { op->setDefined(true, true); }

    void memberHasBecomeUndefined(UInt index) {
        if (!op->evaluationComplete) {
            return;
        }
        op->removeSingleValue(previousValues.get(index));
        if (op->operand->view().numberUndefined == 1) {
            op->setDefined(false, false);
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers, true);
        }
    }

    void memberHasBecomeDefined(UInt index) {
        if (!op->evaluationComplete) {
            if (op->operand->view().numberUndefined == 0) {
                op->setDefined(true, true);
            }
            return;
        }

        op->addSingleValue(
            op->operand->view().getMembers<IntView>()[index]->view().value);
        if (op->operand->view().numberUndefined == 0) {
            op->setDefined(true, false);
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers,
                          true);
        }
    }
};

void OpProd::reevaluate() {
    setDefined(true, false);
    value = 1;
    cachedValue = 1;
    numberZeros = 0;
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        if (operandChild->isUndefined()) {
            setDefined(false, false);
        } else {
            addSingleValue(operandChild->view().value);
        }
    }
    evaluationComplete = true;
}

void OpProd::updateVarViolations(const ViolationContext& vioContext,
                                 ViolationContainer& vioDesc) {
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        operandChild->updateVarViolations(vioContext, vioDesc);
    }
}

void OpProd::copy(OpProd& newOp) const {
    newOp.value = value;
    newOp.cachedValue = cachedValue;
    newOp.numberZeros = numberZeros;
    newOp.evaluationComplete = this->evaluationComplete;
}

std::ostream& OpProd::dumpState(std::ostream& os) const {
    os << "OpProd: value=" << value << endl;
    return operand->dumpState(os);
}

bool OpProd::optimiseImpl() { return flatten<IntView>(*this); }
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpProd> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};

ExprRef<IntView> OpMaker<OpProd>::make(ExprRef<SequenceView> o) {
    return make_shared<OpProd>(move(o));
}
