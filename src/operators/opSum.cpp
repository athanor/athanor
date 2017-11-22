#include "operators/opSum.h"
#include <cassert>
using namespace std;
void evaluate(OpSum& op) {
    op.value = 0;
    for (auto& operand : op.operands) {
        evaluate(operand);
        op.value += getView<IntView>(operand).value;
    }
}

class OpSumTrigger : public IntTrigger {
   public:
    friend OpSum;
    OpSum* op;
    int64_t lastMemberValue;

   public:
    OpSumTrigger(OpSum* op) : op(op) {}
    void possibleValueChange(int64_t oldValue) { lastMemberValue = oldValue; }
    void valueChanged(int64_t newValue) {
        if (newValue == lastMemberValue) {
            return;
        }
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->value); },
            op->triggers);
        op->value -= lastMemberValue;
        op->value += newValue;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers);
    }
};

class OpSumIterAssignedTrigger : public IterAssignedTrigger<IntValue>,
                                 public OpSumTrigger {
    using OpSumTrigger::OpSumTrigger;
    void iterHasNewValue(const IntValue& oldValue,
                         const ValRef<IntValue>& newValue) {
        possibleValueChange(oldValue.value);
        valueChanged(newValue->value);
    }
};

OpSum::OpSum(OpSum&& other)
    : IntView(move(other)),
      operands(move(other.operands)),
      operandTrigger(move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}

void startTriggering(OpSum& op) {
    op.operandTrigger = make_shared<OpSumIterAssignedTrigger>(&op);
    for (auto& operand : op.operands) {
        addTrigger<IntTrigger>(getView<IntView>(operand).triggers,
                               op.operandTrigger);
        mpark::visit(overloaded(
                         [&](IterRef<IntValue>& ref) {
                             addTrigger<IterAssignedTrigger<IntValue>>(
                                 ref.getIterator().unrollTriggers,
                                 op.operandTrigger);
                         },
                         [](auto&) {}),
                     operand);
        startTriggering(operand);
    }
}

void stopTriggering(OpSum& op) {
    if (op.operandTrigger) {
        deleteTrigger(op.operandTrigger);
    }
    for (auto& operand : op.operands) {
        stopTriggering(operand);
    }
}

void updateViolationDescription(const OpSum& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    for (auto& operand : op.operands) {
        updateViolationDescription(operand, parentViolation, vioDesc);
    }
}

shared_ptr<OpSum> deepCopyForUnroll(const OpSum& op,
                                    const IterValue& iterator) {
    vector<IntReturning> operands;
    operands.reserve(op.operands.size());
    for (auto& operand : op.operands) {
        operands.emplace_back(deepCopyForUnroll(operand, iterator));
    }
    auto newOpSum = make_shared<OpSum>(move(operands));
    newOpSum->value = op.value;
    return newOpSum;
}
