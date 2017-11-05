#include <cassert>
#include "operators/intOperators.h"
using namespace std;
void evaluate(OpSum& op) {
    op.value = 0;
    for (auto& operand : op.operands) {
        evaluate(operand);
        op.value += getIntView(operand).value;
    }
}

class OpSumTrigger : public IntTrigger {
    OpSum& op;
    int64_t lastMemberValue;

   public:
    OpSumTrigger(OpSum& op) : op(op) {}
    void possibleValueChange(int64_t oldValue) { lastMemberValue = oldValue; }
    void valueChanged(int64_t newValue) {
        if (newValue == lastMemberValue) {
            return;
        }
        for (auto& trigger : op.triggers) {
            trigger->possibleValueChange(op.value);
        }
        op.value -= lastMemberValue;
        op.value += newValue;
        for (auto& trigger : op.triggers) {
            trigger->valueChanged(op.value);
        }
    }
};

void startTriggering(OpSum& op) {
    auto trigger = std::make_shared<OpSumTrigger>(op);
    for (auto& operand : op.operands) {
        getIntView(operand).triggers.emplace_back(trigger);
        startTriggering(operand);
    }
}

void stopTriggering(OpSum& op) {
    assert(false);  // todo
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
