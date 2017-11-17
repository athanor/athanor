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
        mpark::visit(overloaded(
                         [&](IterRef<BoolValue>& ref) {
                             auto unrollTrigger =
                                 make_shared<OpAndIterAssignedTrigger>(&op, i);
                             addTrigger<IterAssignedTrigger<BoolValue>>(
                                 ref.getIterator().unrollTriggers,
                                 unrollTrigger);
                         },
                         [](auto&) {}),
                     operand);
        startTriggering(operand);
    }
}

void stopTriggering(OpAnd& op) {
    while (!op.operandTriggers.empty()) {
        auto& operand = op.operands[op.operandTriggers.size() - 1];
        deleteTrigger(op.operandTriggers.back());
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

std::shared_ptr<OpAnd> deepCopyForUnroll(const OpAnd& op,
                                         const IterValue& iterator) {
    std::vector<BoolReturning> operands;
    operands.reserve(op.operands.size());
    for (auto& operand : op.operands) {
        operands.emplace_back(deepCopyForUnroll(operand, iterator));
    }
    auto newOpAnd =
        std::make_shared<OpAnd>(std::move(operands), op.violatingOperands);
    newOpAnd->violation = op.violation;
    startTriggering(*newOpAnd);
    return newOpAnd;
}
