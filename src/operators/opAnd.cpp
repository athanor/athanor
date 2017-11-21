#include "operators/opAnd.h"
#include <cassert>
#include "utils/ignoreUnused.h"
using namespace std;
void attachTriggers(OpAnd& op);
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
    attachTriggers(op);
}

OpAnd::OpAnd(OpAnd&& other)
    : BoolView(move(other)),
      operands(move(other.operands)),
      violatingOperands(move(other.violatingOperands)),
      operandTriggers(move(other.operandTriggers)) {
    setTriggerParent(this, operandTriggers);
}

void attachTriggers(OpAnd& op) {
    for (size_t i = 0; i < op.operands.size(); ++i) {
        auto& operand = op.operands[i];

        mpark::visit(
            overloaded(
                [&](IterRef<BoolValue>& ref) {
                    auto unrollTrigger =
                        make_shared<OpAndIterAssignedTrigger>(&op, i);
                    addTrigger<IterAssignedTrigger<BoolValue>>(
                        ref.getIterator().unrollTriggers, unrollTrigger);
                    addTrigger<BoolTrigger>(getView<BoolView>(operand).triggers,
                                            unrollTrigger);
                    op.operandTriggers.emplace_back(move(unrollTrigger));
                },
                [&](auto&) {
                    auto trigger = make_shared<OpAndTrigger>(&op, i);
                    addTrigger<BoolTrigger>(getView<BoolView>(operand).triggers,
                                            trigger);
                    op.operandTriggers.emplace_back(move(trigger));
                }),
            operand);
    }
}

void stopTriggering(OpAnd& op) {
    while (!op.operandTriggers.empty()) {
        deleteTrigger(op.operandTriggers.back());
        op.operandTriggers.pop_back();
    }
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

shared_ptr<OpAnd> deepCopyForUnroll(const OpAnd& op,
                                    const IterValue& iterator) {
    vector<BoolReturning> operands;
    operands.reserve(op.operands.size());
    for (auto& operand : op.operands) {
        operands.emplace_back(deepCopyForUnroll(operand, iterator));
    }
    auto newOpAnd = make_shared<OpAnd>(move(operands), op.violatingOperands);
    newOpAnd->violation = op.violation;
    attachTriggers(*newOpAnd);
    return newOpAnd;
}
