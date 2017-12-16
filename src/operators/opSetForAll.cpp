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
                op.unroll(ref, false, true);
            }
        },
        members);
}

struct OpSetForAll::ContainerTrigger : public SetTrigger {
    OpSetForAll* op;
    u_int64_t lastMemberHash = 0;
    ContainerTrigger(OpSetForAll* op) : op(op) {}

    inline void valueRemoved(const AnyValRef& val) final { op->roll(val); }

    inline void valueAdded(const AnyValRef& val) final {
        op->valuesToUnroll.emplace_back(move(val));
        if (op->valuesToUnroll.size() == 1) {
            addDelayedTrigger(make_shared<DelayedUnrollTrigger>(op));
        }
    }

    inline void possibleValueChange(const AnyValRef& oldValue) {
        lastMemberHash = getValueHash(oldValue);
    }
    inline void valueChanged(const AnyValRef& newValue) {
        op->valueChanged(lastMemberHash, getValueHash(newValue));
    }

    void iterHasNewValue(const SetValue& oldValue,
                         const ValRef<SetValue>& newValue) {
        deepCopy(oldValue, *newValue);
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
void stopTriggering(OpSetForAll& op) { op.stopTriggeringBase(); }
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

std::ostream& dumpState(std::ostream& os, const OpSetForAll& op) {
    os << "OpSetForAll: violation=" << op.violation << "\noperands [";
    bool first = true;
    for (auto& exprValuePair : op.unrolledExprs) {
        if (first) {
            first = false;
        } else {
            os << ",\n";
        }
        dumpState(os, exprValuePair.first);
    }
    os << "]";
    return os;
}
