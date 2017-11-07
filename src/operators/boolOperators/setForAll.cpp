#include <cassert>
#include "operators/boolOperators.h"
using namespace std;
/*
void evaluate(OpAnd& op) {
    op.violation = 0;
    for (size_t i = 0; i < op.operands.size(); ++i) {
        auto& operand = op.operands[i];
        evaluate(operand);
        u_int64_t violation = getView<BoolView>(operand).violation;
        if (violation > 0) {
            op.violatingOperands.insert(i);
        }
        op.violation += getView<BoolView>(operand).violation;
    }
}

class OpAndTrigger : public BoolTrigger {
    OpAnd& op;
    const size_t index;
    u_int64_t lastViolation;

   public:
    OpAndTrigger(OpAnd& op, size_t index) : op(op), index(index) {}
    void possibleValueChange(u_int64_t oldVilation) {
        lastViolation = oldVilation;
    }
    void valueChanged(u_int64_t newViolation) {
        if (newViolation == lastViolation) {
            return;
        }
        if (newViolation > 0 && lastViolation == 0) {
            op.violatingOperands.insert(index);
        } else if (newViolation == 0 && lastViolation > 0) {
            op.violatingOperands.erase(index);
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
    for (size_t i = 0; i < op.operands.size(); ++i) {
        auto& operand = op.operands[i];
        auto trigger = make_shared<OpAndTrigger>(op, i);
        getView<BoolView>(operand).triggers.emplace_back(trigger);
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
    for (size_t violatingOperandIndex : op.violatingOperands) {
        updateViolationDescription(op.operands[violatingOperandIndex],
                                   op.violation, vioDesc);
    }
}
*/
