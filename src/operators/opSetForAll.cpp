#include "operators/opSetForAll.h"
#include <cassert>
#include "operators/opAnd.h"
#include "utils/ignoreUnused.h"
using ExprTrigger = OpSetForAll::ExprTrigger;
using ExprIterAssignedTrigger = OpSetForAll::ExprIterAssignedTrigger;
using namespace std;
using ContainerTrigger = OpSetForAll::ContainerTrigger;
using ContainerIterAssignedTrigger = OpSetForAll::ContainerIterAssignedTrigger;
using DelayedUnrollTrigger = OpSetForAll::DelayedUnrollTrigger;
void startTriggeringExpr(OpSetForAll& op, BoolReturning& operand, size_t index);
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

struct OpSetForAll::ContainerTrigger : public SetTrigger {
    OpSetForAll* op;
    ContainerTrigger(OpSetForAll* op) : op(op) {}

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
        op->exprTriggers[indexExprPair.first] = move(op->exprTriggers.back());
        op->exprTriggers.pop_back();
        op->exprTriggers[indexExprPair.first] = move(op->exprTriggers.back());
        op->exprTriggers.pop_back();
    }

    inline void valueAdded(const Value& val) final {
        op->queueOfValuesToAdd.emplace_back(move(val));
        if (op->queueOfValuesToAdd.size() == 1) {
            addDelayedTrigger(make_shared<DelayedUnrollTrigger>(op));
        }
    }

    inline void possibleValueChange(const Value&) {}
    inline void valueChanged(const Value&) {}
};

struct OpSetForAll::ContainerIterAssignedTrigger
    : public ContainerTrigger,
      public IterAssignedTrigger<SetValue> {
    using ContainerTrigger::ContainerTrigger;

    void iterHasNewValue(const SetValue& oldValue,
                         const ValRef<SetValue>& newValue) {
        ignoreUnused(oldValue, newValue);
        assert(false);
    }
};
struct OpSetForAll::DelayedUnrollTrigger : public DelayedTrigger {
    OpSetForAll* op;

    DelayedUnrollTrigger(OpSetForAll* op) : op(op) {}
    void trigger() final {
        while (!op->valuesToUnroll.empty()) {
            op->unroll(op->valuesToUnroll.back());
            op->valuesToUnroll.pop_back();
            startTriggeringExpr(*op, op->unrolledExprs.back().first,
                                op->unrolledExprs.size() - 1);
        }
    }
};

OpSetForAll::OpSetForAll(OpSetForAll&& other)
    : BoolView(move(other)),
      Quantifier(move(other)),
      violatingOperands(move(other.violatingOperands)),
      exprTriggers(move(other.exprTriggers)),
      containerTrigger(move(other.containerTrigger)),
      containerIterAssignedTrigger(move(other.containerIterAssignedTrigger)),
      delayedUnrollTrigger(move(other.delayedUnrollTrigger)),
      valuesToUnroll(move(other.valuesToUnroll)) {
    setTriggerParent(this, exprTriggers, containerTrigger,
                     containerIterAssignedTrigger, delayedUnrollTrigger);
}

void startTriggering(OpSetForAll& op) {
    op.containerTrigger = make_shared<ContainerTrigger>(&op);
    addTrigger<SetTrigger>(getView<SetView>(op.container).triggers,
                           op.containerTrigger);

    mpark::visit(overloaded(
                     [&](IterRef<SetValue>& ref) {
                         op.containerIterAssignedTrigger =
                             make_shared<ContainerIterAssignedTrigger>(&op);
                         addTrigger<IterAssignedTrigger<SetValue>>(
                             ref.getIterator().unrollTriggers,
                             op.containerIterAssignedTrigger);
                     },
                     [](auto&) {}),
                 op.container);
    for (size_t i = 0; i < op.unrolledExprs.size(); ++i) {
        startTriggeringExpr(op, op.unrolledExprs[i].first, i);
    }
}

void startTriggeringExpr(OpSetForAll& op, BoolReturning& operand,
                         size_t index) {
    auto trigger = make_shared<ExprTrigger>(&op, index);
    addTrigger<BoolTrigger>(getView<BoolView>(operand).triggers, trigger);
    op.exprTriggers.emplace_back(trigger);
    mpark::visit(overloaded(
                     [&](IterRef<BoolValue>& ref) {
                         auto unrollTrigger =
                             make_shared<ExprIterAssignedTrigger>(&op, index);
                         addTrigger<IterAssignedTrigger<BoolValue>>(
                             ref.getIterator().unrollTriggers, unrollTrigger);
                         op.exprTriggers.emplace_back(move(unrollTrigger));
                     },
                     [](auto&) {}),
                 operand);
    startTriggering(operand);
}

/*
void stopTriggering(OpSetForAll& op) {
    while (!op.operandTriggers.empty()) {
        auto& operand = op.operands[op.operandTriggers.size() - 1];
        deleteTrigger(op.operandTriggers.back());
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

shared_ptr<OpSetForAll> deepCopyForUnroll(const OpSetForAll& op,
                                         const IterValue& iterator) {
    vector<BoolReturning> operands;
    operands.reserve(op.operands.size());
    for (auto& operand : op.operands) {
        operands.emplace_back(deepCopyForUnroll(operand, iterator));
    }
    auto newOpSetForAll =
        make_shared<OpSetForAll>(move(operands),
op.violatingOperands); newOpSetForAll->violation = op.violation;
    startTriggering(*newOpSetForAll);
    return newOpSetForAll;
}
*/
