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
    OpProd* op;
    OperandsSequenceTrigger(OpProd* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        if (!op->evaluationComplete) {
            return;
        }
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        if (expr->isUndefined()) {
            if (op->operand->view().numberUndefined == 1) {
                op->cachedValues.insert(index, 1);
                op->setDefined(false, false);
                visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                              op->triggers, true);
            }
            return;
        }
        Int operandValue = expr->view().value;
        op->cachedValues.insert(index, operandValue);
        op->changeValue([&]() {
            op->addSingleValue(operandValue);
            return true;
        });
    }

    void valueRemoved(UInt index, const AnyExprRef& exprIn) final {
        if (!op->evaluationComplete) {
            return;
        }

        const auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        Int operandValue = op->cachedValues.erase(index);

        if (expr->isUndefined()) {
            if (op->operand->view().numberUndefined == 0) {
                op->setDefined(true, false);
                visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                              op->triggers, true);
            }
            return;
        }
        op->changeValue([&]() {
            op->removeSingleValue(operandValue);
            return true;
        });
    }

    inline void positionsSwapped(UInt index1, UInt index2) {
        swap(op->cachedValues.get(index1), op->cachedValues.get(index2));
    }

    ExprRef<IntView>& getMember(UInt index) {
        return op->operand->view().getMembers<IntView>()[index];
    }
    Int getValueCatchUndef(UInt index) {
        auto& member = getMember(index);
        return (member->isUndefined()) ? 1 : member->view().value;
    }

    inline void handleSingleOperandChange(UInt index) {
        debug_code(assert((op->numberZeros > 0 && op->value == 0) ||
                          (op->value != 0 && op->numberZeros == 0)));
        Int newValue = getValueCatchUndef(index);
        Int oldValue = op->cachedValues.getAndSet(index, newValue);
        op->removeSingleValue(oldValue);
        op->addSingleValue(newValue);
    }

    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        if (!op->evaluationComplete) {
            return;
        }
        op->changeValue([&]() {
            for (size_t i = startIndex; i < endIndex; i++) {
                handleSingleOperandChange(i);
            }
            return true;
        });
    }

    void valueChanged() final {
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

    void hasBecomeUndefined() final { op->setDefined(false, true); }
    void hasBecomeDefined() final { op->setDefined(true, true); }

    void memberHasBecomeUndefined(UInt index) {
        if (!op->evaluationComplete) {
            return;
        }
        op->removeSingleValue(op->cachedValues.get(index));
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
    cachedValues.clear();
    numberZeros = 0;
    auto& members = operand->view().getMembers<IntView>();
    for (size_t index = 0; index < members.size(); index++) {
        auto& operandChild = members[index];
        if (operandChild->isUndefined()) {
            setDefined(false, false);
            cachedValues.insert(index, 1);
        } else {
            Int operandValue = operandChild->view().value;
            addSingleValue(operandValue);
            cachedValues.insert(index, operandValue);
        }
    }
    evaluationComplete = true;
}

void OpProd::updateVarViolationsImpl(const ViolationContext& vioContext,
                                     ViolationContainer& vioContainer) {
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        operandChild->updateVarViolations(vioContext, vioContainer);
    }
}

void OpProd::copy(OpProd& newOp) const {
    newOp.value = value;
    newOp.cachedValues = cachedValues;
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
