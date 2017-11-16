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
   protected:
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
            op->triggers, emptyEndOfTriggerQueue);
        op->value -= lastMemberValue;
        op->value += newValue;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers, emptyEndOfTriggerQueue);
    }
};

class OpSumIterValueChangeTrigger : public IterValueChangeTrigger<IntValue>,
                                    public OpSumTrigger {
    using OpSumTrigger::OpSumTrigger;
    void iterHasNewValue(const IntValue& oldValue,
                         const ValRef<IntValue>& newValue) {
        possibleValueChange(oldValue.value);
        valueChanged(newValue->value);
    }
};

OpSum::OpSum(OpSum&& other)
    : IntView(std::move(other)),
      operands(std::move(other.operands)),
      operandTrigger(std::move(other.operandTrigger)),
      operandIterValueChangeTrigger(
          std::move(other.operandIterValueChangeTrigger)) {
    if (operandTrigger) {
        operandTrigger->op = this;
    }
    if (operandIterValueChangeTrigger) {
        operandIterValueChangeTrigger->op = this;
    }
}

void startTriggering(OpSum& op) {
    op.operandTrigger = std::make_shared<OpSumTrigger>(&op);
    for (auto& operand : op.operands) {
        addTrigger<IntTrigger>(getView<IntView>(operand).triggers,
                               op.operandTrigger);
        startTriggering(operand);
        mpark::visit(
            overloaded(
                [&](IterRef<IntValue>& ref) {
                    if (!op.operandIterValueChangeTrigger) {
                        op.operandIterValueChangeTrigger =
                            std::make_shared<OpSumIterValueChangeTrigger>(&op);
                    }
                    addTrigger<IterValueChangeTrigger<IntValue>>(
                        ref.getIterator().unrollTriggers,
                        op.operandIterValueChangeTrigger);
                },
                [](auto&) {}),
            operand);
    }
}

void stopTriggering(OpSum& op) {
    if (op.operandTrigger) {
        deleteTrigger<IntTrigger>(op.operandTrigger);
    }
    if (op.operandIterValueChangeTrigger) {
        deleteTrigger<IterValueChangeTrigger<IntValue>>(
            op.operandIterValueChangeTrigger);
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

std::shared_ptr<OpSum> deepCopyForUnroll(const OpSum& op,
                                         const IterValue& iterator) {
    std::vector<IntReturning> operands;
    operands.reserve(op.operands.size());
    for (auto& operand : op.operands) {
        operands.emplace_back(deepCopyForUnroll(operand, iterator));
    }
    auto newOpSum = std::make_shared<OpSum>(std::move(operands));
    newOpSum->value = op.value;
    return newOpSum;
}
