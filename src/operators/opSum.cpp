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

OpSum::OpSum(OpSum&& other)
    : IntView(std::move(other)),
      operands(std::move(other.operands)),
      operandTrigger(std::move(other.operandTrigger)) {
    operandTrigger->op = this;
}

void startTriggering(OpSum& op) {
    op.operandTrigger = std::make_shared<OpSumTrigger>(&op);
    for (auto& operand : op.operands) {
        addTrigger<IntTrigger>(getView<IntView>(operand).triggers,
                               op.operandTrigger);
        startTriggering(operand);
    }
}

void stopTriggering(OpSum& op) {
    for (auto& operand : op.operands) {
        deleteTrigger<IntTrigger>(getView<IntView>(operand).triggers,
                                  op.operandTrigger);
        stopTriggering(operand);
    }
}

void updateViolationDescription(const OpSum& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    for (auto& operand : op.operands) {
        updateViolationDescription(operand, parentViolation, vioDesc);
    }
}
