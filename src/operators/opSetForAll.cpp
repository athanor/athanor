#include "operators/opSetForAll.h"
#include <cassert>

using namespace std;
void evaluate(OpSetForAll& op) {
    SetMembersVector members = evaluate(op.container);
    op.violation = 0;
    mpark::visit(
        [&](auto& membersImpl) {
            op.violatingOperands =
                FastIterableIntSet(0, membersImpl.size() - 1);
            for (auto& ref : membersImpl) {
                op.unroll(ref);
                auto& operand = op.unrolledExprs.back().first;
                evaluate(operand);
                u_int64_t violation = getView<BoolView>(operand).violation;
                if (violation > 0) {
                    op.violatingOperands.insert(op.unrolledExprs.size() - 1);
                    op.violation += getView<BoolView>(operand).violation;
                }
            }
        },
        members);
}

class OpSetForAllContainerTrigger : public SetTrigger {
    friend OpSetForAll;
    OpSetForAll* op;
    OpSetForAllContainerTrigger(OpSetForAll* op) : op(op) {}

    inline void valueRemoved(const Value& val) final {
        auto indexExprPair = op->roll(val);
        u_int64_t removedViolation =
            getView<BoolView>(indexExprPair.second).violation;
        if (removedViolation > 0) {
            visitTriggers(
                [&](auto& trigger) {
                    trigger->possibleValueChange(op->violation);
                },
                op->triggers);
            op->violation -= removedViolation;
            op->violatingOperands.erase(indexExprPair.first);
            visitTriggers(
                [&](auto& trigger) { trigger->valueChanged(op->violation); },
                op->triggers);
        }
        if (op->violatingOperands.erase(op->unrolledExprs.size())) {
            op->violatingOperands.insert(indexExprPair.first);
        }
        deleteTrigger(op->exprTriggers[indexExprPair.first]);
        op->exprTriggers[indexExprPair.first] =
            std::move(op->exprTriggers.back());
        op->exprTriggers.pop_back();
    }

    inline void valueAdded(const Value& val) final {
        op->queueOfValuesToAdd.emplace_back(std::move(val));
    }

    inline void possibleValueChange(const Value&) {}
    inline void valueChanged(const Value&) {}
};

class OpSetForAllContainerDelayedTrigger : public DelayedTrigger {
    friend OpSetForAll;
    OpSetForAll* op;
    OpSetForAllContainerDelayedTrigger(OpSetForAll* op) : op(op) {}
    void trigger() final {}
};
OpSetForAll::OpSetForAll(OpSetForAll&& other)
    : BoolView(std::move(other)),
      Quantifier(std::move(other)),
      violatingOperands(std::move(other.violatingOperands)),
      exprTriggers(std::move(other.exprTriggers)),
      containerTrigger(std::move(other.containerTrigger)),
      containerIterAssignedTrigger(
          std::move(other.containerIterAssignedTrigger)),
      containerDelayedTrigger(std::move(other.containerDelayedTrigger)) {
    for (auto& trigger : exprTriggers) {
        trigger->op = this;
    }
    if (containerTrigger) {
        containerTrigger->op = this;
    }
    if (containerIterAssignedTrigger) {
        containerIterAssignedTrigger->op = this;
    }
    if (containerDelayedTrigger) {
        containerDelayedTrigger->op = this;
    }
}
/*
void startTriggering(OpSetForAll& op) {
    for (size_t i = 0; i < op.operands.size(); ++i) {
        auto& operand = op.operands[i];
        auto trigger = make_shared<OpSetForAllTrigger>(&op, i);
        addTrigger<BoolTrigger>(getView<BoolView>(operand).triggers, trigger);
        op.operandTriggers.emplace_back(trigger);
        startTriggering(operand);
    }
}

void stopTriggering(OpSetForAll& op) {
    while (!op.operandTriggers.empty()) {
        auto& operand = op.operands[op.operandTriggers.size() - 1];
        deleteTrigger<BoolTrigger>(getView<BoolView>(operand).triggers,

                                   op.operandTriggers.back());
        op.operandTriggers.pop_back();
        stopTriggering(operand);
    }
}

void updateViolationDescription(const OpSetForAll& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    for (size_t violatingOperandIndex : op.violatingOperands) {
        updateViolationDescription(op.operands[violatingOperandIndex],
                                   op.violation, vioDesc);
    }
}

std::shared_ptr<OpSetForAll> deepCopyForUnroll(const OpSetForAll& op,
                                               const IterValue& iterator) {
    ignoreUnused(op, iterator);
    abort();
}
* /

    void evaluate(&op) {
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
*/
