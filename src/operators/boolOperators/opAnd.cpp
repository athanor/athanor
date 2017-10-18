#include <cassert>
#include "operators/boolOperators.h"
using namespace std;

BoolView getBoolView(OpAnd& op) { return BoolView(op.violation, op.triggers); }

void evaluate(OpAnd& op) {
    op.violation = 0;
    for (auto& operand : op.operands) {
        evaluate(operand);
        op.violation += getBoolView(operand).violation;
    }
}

class OpAndTrigger : public BoolTrigger {
    OpAnd& op;
    u_int64_t lastViolation;

   public:
    OpAndTrigger(OpAnd& op) : op(op) {}
    void possibleValueChange(u_int64_t oldVilation) {
        lastViolation = oldVilation;
    }
    void valueChanged(u_int64_t newViolation) {
        if (newViolation == lastViolation) {
            return;
        }
        for (auto& trigger : op.triggers) {
            trigger->possibleValueChange(op.violation);
        }
        op.violation -= lastViolation;
        op.violation += newViolation;
        for (auto& trigger : op.triggers) {
            trigger->valueChanged(op.violation);
        }
    }
};

void startTriggering(OpAnd& op) {
    auto trigger = make_shared<OpAndTrigger>(op);
    for (auto& operand : op.operands) {
        getBoolView(operand).triggers.emplace_back(trigger);
        startTriggering(operand);
    }
}

void stopTriggering(OpAnd& op) {
    assert(false);  // todo
    for (auto& operand : op.operands) {
        stopTriggering(operand);
    }
}

void updateViolationDescription(const OpAnd& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    for (auto& operand : op.operands) {
        if (getBoolView(operand).violation != 0) {
            updateViolationDescription(operand, op.violation, vioDesc);
        }
    }
}
