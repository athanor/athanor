#include "operators/opAnd.h"
#include <cassert>
#include "utils/ignoreUnused.h"
using namespace std;

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
    friend OpAnd;
    OpAnd* op;
    const size_t index;
    u_int64_t lastViolation;

   public:
    OpAndTrigger(OpAnd* op, size_t index) : op(op), index(index) {}
    void possibleValueChange(u_int64_t oldVilation) {
        lastViolation = oldVilation;
    }
    void valueChanged(u_int64_t newViolation) {
        if (newViolation == lastViolation) {
            return;
        }
        if (newViolation > 0 && lastViolation == 0) {
            op->violatingOperands.insert(index);
        } else if (newViolation == 0 && lastViolation > 0) {
            op->violatingOperands.erase(index);
        }
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->violation); },
            op->triggers, emptyEndOfTriggerQueue);
        op->violation -= lastViolation;
        op->violation += newViolation;
        visitTriggers(
            [&](auto& trigger) { trigger->valueChanged(op->violation); },
            op->triggers, emptyEndOfTriggerQueue);
    }
};

OpAnd::OpAnd(OpAnd&& other)
    : BoolView(std::move(other)),
      operands(std::move(other.operands)),
      violatingOperands(std::move(other.violatingOperands)),
      operandTriggers(std::move(other.operandTriggers)) {
    for (auto& trigger : operandTriggers) {
        trigger->op = this;
    }
}

void startTriggering(OpAnd& op) {
    for (size_t i = 0; i < op.operands.size(); ++i) {
        auto& operand = op.operands[i];
        auto trigger = make_shared<OpAndTrigger>(&op, i);
        addTrigger<BoolTrigger>(getView<BoolView>(operand).triggers, trigger);
        op.operandTriggers.emplace_back(trigger);
        startTriggering(operand);
    }
}

void stopTriggering(OpAnd& op) {
    while (!op.operandTriggers.empty()) {
        auto& operand = op.operands[op.operandTriggers.size() - 1];
        deleteTrigger<BoolTrigger>(getView<BoolView>(operand).triggers,

                                   op.operandTriggers.back());
        op.operandTriggers.pop_back();
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

std::shared_ptr<OpAnd> deepCopyForUnroll(
    const OpAnd& op, const QuantValue& unrollingQuantifier) {
    ignoreUnused(op, unrollingQuantifier);
    abort();
}
