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
    OpSum* op;
    OperandsSequenceTrigger(OpSum* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        if (!op->evaluationComplete) {
            return;
        }
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        if (expr->isUndefined()) {
            op->cachedValues.insert(index, 0);
            if (op->operand->view().numberUndefined == 1) {
                op->setDefined(false, false);
                visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                              op->triggers, true);
            }
            return;
        }
        auto& operandValue = expr->view().value;
        op->cachedValues.insert(index, operandValue);
        op->changeValue([&]() {
            op->value += operandValue;
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
            op->value -= operandValue;
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
        return (member->isUndefined()) ? 0 : member->view().value;
    }

    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        if (!op->evaluationComplete) {
            return;
        }

        Int valueToAdd = 0, valueToRemove = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            Int operandValue = getValueCatchUndef(i);
            valueToAdd += operandValue;
            valueToRemove += op->cachedValues.getAndSet(i, operandValue);
        }
        op->changeValue([&]() {
            op->value -= valueToRemove;
            op->value += valueToAdd;
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
        op->value -= op->cachedValues.get(index);
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
        Int operandValue =
            op->operand->view().getMembers<IntView>()[index]->view().value;
        op->value += operandValue;
        op->cachedValues.set(index, operandValue);
        if (op->operand->view().numberUndefined == 0) {
            op->setDefined(true, false);
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers,
                          true);
        }
    }
};

void OpSum::reevaluate() {
    setDefined(true, false);
    value = 0;
    auto& members = operand->view().getMembers<IntView>();
    for (size_t index = 0; index < members.size(); index++) {
        auto& operandChild = members[index];
        if (operandChild->isUndefined()) {
            cachedValues.insert(index, 0);
            setDefined(false, false);
        } else {
            Int operandValue = operandChild->view().value;
            value += operandValue;
            cachedValues.insert(index, operandValue);
        }
    }
    evaluationComplete = true;
}

void OpSum::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        operandChild->updateVarViolations(vioContext, vioContainer);
    }
}

void OpSum::copy(OpSum& newOp) const {
    newOp.value = value;
    newOp.cachedValues = cachedValues;
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
