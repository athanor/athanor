#include "operators/opSetForAll.h"
#include <cassert>

#include "operators/opAnd.h"
#include "utils/ignoreUnused.h"
using namespace std;
using ContainerTrigger = OpSetForAll::ContainerTrigger;
using DelayedUnrollTrigger = OpSetForAll::DelayedUnrollTrigger;

void evaluate(OpSetForAll& op) {
    SetMembersVector members = evaluate(op.container);
    op.violation = 0;
    mpark::visit(
        [&](auto& membersImpl) {
            op.violatingOperands =
                FastIterableIntSet(0, membersImpl.size() - 1);
            for (auto& ref : membersImpl) {
                auto& operand = op.unroll(ref, false, true).second;
                u_int64_t violation = getView<BoolView>(operand).violation;
                if (violation > 0) {
                    op.violation += getView<BoolView>(operand).violation;
                }
            }
        },
        members);
}

struct OpSetForAll::ContainerTrigger : public SetTrigger,
                                       public IterAssignedTrigger<SetValue> {
    OpSetForAll* op;
    ContainerTrigger(OpSetForAll* op) : op(op) {}

    inline void valueRemoved(const AnyValRef& val) final {
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
            visitTriggers(
                [&](auto& trigger) { trigger->valueChanged(op->violation); },
                op->triggers);
        }
    }

    inline void valueAdded(const AnyValRef& val) final {
        op->valuesToUnroll.emplace_back(move(val));
        if (op->valuesToUnroll.size() == 1) {
            addDelayedTrigger(make_shared<DelayedUnrollTrigger>(op));
        }
    }

    inline void possibleValueChange(const AnyValRef&) {}
    inline void valueChanged(const AnyValRef&) {}
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
            auto expr = op->unroll(op->valuesToUnroll.back());
            op->valuesToUnroll.pop_back();
            u_int64_t violation = getView<BoolView>(expr.second).violation;
            if (violation > 0) {
                op->violation += violation;
            }
        }
    }
};

OpSetForAll::OpSetForAll(OpSetForAll&& other)
    : BoolView(move(other)),
      BoolQuantifier(move(other)),
      containerTrigger(move(other.containerTrigger)),
      delayedUnrollTrigger(move(other.delayedUnrollTrigger)),
      valuesToUnroll(move(other.valuesToUnroll)) {
    setTriggerParent(this, exprTriggers, containerTrigger,
                     delayedUnrollTrigger);
}

void startTriggering(OpSetForAll& op) { op.startTriggeringBase(); }
void stopTriggering(OpSetForAll op) { op.stopTriggeringBase(); }
void updateViolationDescription(const OpSetForAll& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    for (size_t violatingOperandIndex : op.violatingOperands) {
        updateViolationDescription(
            op.unrolledExprs[violatingOperandIndex].first, op.violation,
            vioDesc);
    }
}


shared_ptr<OpSetForAll> deepCopyForUnroll(const OpSetForAll& op,
                                          const AnyIterRef& iterator) {
    auto newOpSetForAll = make_shared<OpSetForAll>(
        op.deepCopyQuantifierForUnroll(iterator), op.violatingOperands);
    newOpSetForAll->violation = op.violation;
    return newOpSetForAll;
}
