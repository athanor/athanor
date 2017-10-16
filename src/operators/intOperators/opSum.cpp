#include <cassert>
#include "operators/intOperators.h"
using namespace std;
IntView getIntView(OpSum& op) { return IntView(op.total, op.triggers); }

void evaluate(OpSum& op) {
    op.total = 0;
    for (auto& operand : op.operands) {
        evaluate(operand);
        op.total += getIntView(operand).value;
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
            trigger->possibleValueChange(op.total);
        }
        op.total -= lastMemberValue;
        op.total += newValue;
        for (auto& trigger : op.triggers) {
            trigger->valueChanged(op.total);
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
