#include "operators/opSum.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "operators/flatten.h"
#include "operators/previousValueCache.h"
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OperatorTrates<OpSum>::OperandsSequenceTrigger;
OpSum::OpSum(OpSum&& other)
    : SimpleUnaryOperator<IntView, SequenceView, OpSum>(std::move(other)),
      evaluationComplete(std::move(other.evaluationComplete)) {}
class OperatorTrates<OpSum>::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    Int previousValue;
    PreviousValueCache<Int> previousValues;
    OpSum* op;
    OperandsSequenceTrigger(OpSum* op) : op(op) {}
    void valueAdded(UInt, const AnyExprRef& exprIn) final {
        if (!op->evaluationComplete) {
            return;
        }
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        if (expr->isUndefined()) {
            if (op->operand->view().numberUndefined == 1) {
                op->setDefined(false, false);
                visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                              op->triggers);
            }
            return;
        }
        op->changeValue([&]() {
            op->value += expr->view().value;
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
                              op->triggers);
            }
            return;
        }
        op->changeValue([&]() {
            op->value -= expr->view().value;
            return true;
        });
    }

    inline void beginSwaps() final { previousValues.clear(); }
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt, UInt) {}

    ExprRef<IntView>& getMember(UInt index) {
        return op->operand->view().getMembers<IntView>()[index];
    }
    Int getValueCatchUndef(UInt index) {
        auto& member = getMember(index);
        return (member->isUndefined()) ? 0 : member->view().value;
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

        previousValue = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            previousValue += getValueCatchUndef(i);
        }
    }

    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        debug_log("subsequence changed " << startIndex << "," << endIndex);
        if (!op->evaluationComplete) {
            return;
        }

        Int newValue = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            newValue += getValueCatchUndef(i);
        }
        Int valueToRemove;
        if (endIndex - startIndex == 1) {
            valueToRemove = previousValues.getAndSet(startIndex, newValue);
            ;
        } else {
            valueToRemove = previousValue;
        }

        if (!op->isDefined()) {
            op->value -= valueToRemove;
            op->value += newValue;
        } else {
            op->changeValue([&]() {
                op->value -= valueToRemove;
                op->value += newValue;
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
                              op->triggers);
            }
            return;
        }

        bool isDefined = op->changeValue([&]() {
            op->reevaluate();
            return op->isDefined();
        });
        if (!isDefined) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
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
        op->value -= previousValues.get(index);
        if (op->operand->view().numberUndefined == 1) {
            op->setDefined(false, false);
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        }
    }

    void memberHasBecomeDefined(UInt index) {
        if (!op->evaluationComplete) {
            if (op->operand->view().numberUndefined == 0) {
                op->setDefined(true, true);
            }
            return;
        }

        op->value +=
            op->operand->view().getMembers<IntView>()[index]->view().value;
        if (op->operand->view().numberUndefined == 0) {
            op->setDefined(true, false);
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                          op->triggers);
        }
    }
};

void OpSum::reevaluate() {
    setDefined(true, false);
    value = 0;
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        if (operandChild->isUndefined()) {
            setDefined(false, false);
        } else {
            value += operandChild->view().value;
        }
    }
    evaluationComplete = true;
}

void OpSum::updateVarViolations(const ViolationContext& vioContext,
                                ViolationContainer& vioDesc) {
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        operandChild->updateVarViolations(vioContext, vioDesc);
    }
}

void OpSum::copy(OpSum& newOp) const {
    newOp.value = value;
    newOp.evaluationComplete = this->evaluationComplete;
}

std::ostream& OpSum::dumpState(std::ostream& os) const {
    os << "OpSum: value=" << value << endl;
    return operand->dumpState(os);
}

bool OpSum::optimiseImpl() { return flatten<IntView>(*this); }
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSum> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};

ExprRef<IntView> OpMaker<OpSum>::make(ExprRef<SequenceView> o) {
    return make_shared<OpSum>(move(o));
}
