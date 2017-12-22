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

    inline void possibleMemberValueChange(const AnyValRef& oldValue) {
        lastMemberHash = getValueHash(oldValue);
    }
    inline void memberValueChanged(const AnyValRef& newValue) {
        op->valueChanged(lastMemberHash, getValueHash(newValue));
    }

    void iterHasNewValue(const SetValue& oldValue,
                         const ValRef<SetValue>& newValue) {
        mpark::visit(
            [&](auto& newValImpl) {
                auto& oldValImpl = mpark::get<BaseType<decltype(newValImpl)>>(
                    oldValue.setValueImpl);
                for (auto& member : oldValImpl.members) {
                    if (op->unrolledExprs.empty()) {
                        // this check is only done as the quantifier op may use
                        // optimisations  such that it does not repopulate with
                        // alll the original exprs.
                        break;
                    }
                    this->valueRemoved(member);
                }
                for (auto& member : newValImpl.members) {
                    this->valueAdded(member);
                }
            },
            newValue->setValueImpl);
    }
};

struct OpSetForAll::DelayedUnrollTrigger : public DelayedTrigger {
    OpSetForAll* op;

    DelayedUnrollTrigger(OpSetForAll* op) : op(op) {}
    void trigger() final {
        while (!op->valuesToUnroll.empty()) {
            op->unroll(op->valuesToUnroll.back());
            op->valuesToUnroll.pop_back();
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
    if (newOpSetForAll->unrolledExprs.size() > 0) {
        newOpSetForAll->violation = op.violation;
        newOpSetForAll->violatingOperands = op.violatingOperands;
    } else {
        newOpSetForAll->violation = 0;
    }
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
