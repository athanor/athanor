#include "operators/opSetIntersect.h"
#include <iostream>
#include <memory>
#include "types/typeOperations.h"

using namespace std;
SetMembersVector evaluate(OpSetIntersect& op) {
    SetMembersVector leftVec = evaluate(op.left);
    SetMembersVector rightVec = evaluate(op.right);
    op.clear();
    SetView& leftSetView = getView<SetView>(op.left);
    SetView& rightSetView = getView<SetView>(op.right);
    SetView& smallSetView = (leftSetView.getMemberHashes().size() <
                             rightSetView.getMemberHashes().size())
                                ? leftSetView
                                : rightSetView;
    SetView& largeSetView = (leftSetView.getMemberHashes().size() <
                             rightSetView.getMemberHashes().size())
                                ? rightSetView
                                : leftSetView;
    for (u_int64_t hash : smallSetView.getMemberHashes()) {
        if (largeSetView.getMemberHashes().count(hash)) {
            op.addHash(hash);
        }
    }
    return mpark::visit(
        [&](auto& leftVecImpl) -> SetMembersVector {
            typedef BaseType<decltype(leftVecImpl)> VecType;
            auto rightVecImpl = mpark::get<VecType>(rightVec);
            VecType returnVec;
            auto& smallerVec = (leftVecImpl.size() > rightVecImpl.size())
                                   ? leftVecImpl
                                   : rightVecImpl;
            for (auto& ref : smallerVec) {
                if (op.containsMember(*ref)) {
                    returnVec.emplace_back(std::move(ref));
                }
            }
            return returnVec;
        },
        leftVec);
}

template <bool left>
class OpSetIntersectTrigger : public SetTrigger {
   public:
    OpSetIntersect* op;
    unordered_set<u_int64_t>::iterator oldHashIter;

   public:
    OpSetIntersectTrigger(OpSetIntersect* op) : op(op) {}
    inline void valueRemoved(u_int64_t hash) final {
        unordered_set<u_int64_t>::iterator hashIter;
        if ((hashIter = op->getMemberHashes().find(hash)) !=
            op->getMemberHashes().end()) {
            op->removeHash(hash);
            visitTriggers([&](auto& trigger) { trigger->valueRemoved(hash); },
                          op->triggers);
        }
    }

    inline void valueAdded(const AnyValRef& member) final {
        SetReturning& unchanged = (left) ? op->right : op->left;
        u_int64_t hash = getValueHash(member);
        if (op->getMemberHashes().count(hash)) {
            return;
        }
        SetView& viewOfUnchangedSet = getView<SetView>(unchanged);
        if (viewOfUnchangedSet.getMemberHashes().count(hash)) {
            op->addHash(hash);
            visitTriggers([&](auto& trigger) { trigger->valueAdded(member); },
                          op->triggers);
        }
    }

    inline void setValueChanged(const SetValue& newValue) final {
        for (u_int64_t hash : op->getMemberHashes()) {
            if (!newValue.getMemberHashes().count(hash)) {
                this->valueRemoved(hash);
            }
        }
        mpark::visit(
            [&](auto& newValImpl) {
                for (auto& member : newValImpl.members) {
                    u_int64_t hash = getValueHash(*member);
                    if (!op->getMemberHashes().count(hash)) {
                        this->valueAdded(member);
                    }
                }
            },
            newValue.setValueImpl);
    }
    inline void possibleMemberValueChange(const AnyValRef& member) final {
        u_int64_t hash = getValueHash(member);
        oldHashIter = op->getMemberHashes().find(hash);
        if (oldHashIter != op->getMemberHashes().end()) {
            visitTriggers(
                [&](auto& trigger) {
                    trigger->possibleMemberValueChange(member);
                },
                op->triggers);
        }
    }

    inline void memberValueChanged(const AnyValRef& member) final {
        u_int64_t newHashOfMember = getValueHash(member);
        if (oldHashIter != op->getMemberHashes().end() &&
            newHashOfMember == *oldHashIter) {
            return;
        }
        SetReturning& unchanged = (left) ? op->right : op->left;
        bool containedInUnchangedSet =
            getView<SetView>(unchanged).getMemberHashes().count(
                newHashOfMember);
        if (oldHashIter != op->getMemberHashes().end()) {
            op->removeHash(*oldHashIter);
            if (containedInUnchangedSet) {
                op->addHash(newHashOfMember);
                visitTriggers(
                    [&](auto& trigger) { trigger->memberValueChanged(member); },
                    op->triggers);
            } else {
                visitTriggers(
                    [&](auto& trigger) { trigger->valueRemoved(*oldHashIter); },
                    op->triggers);
            }
        } else if (containedInUnchangedSet) {
            op->addHash(newHashOfMember);
            visitTriggers([&](auto& trigger) { trigger->valueAdded(member); },
                          op->triggers);
        }
    }
    inline void iterHasNewValue(const SetValue&, const ValRef<SetValue>&) {
        assert(false);
    }
};

OpSetIntersect::OpSetIntersect(OpSetIntersect&& other)
    : SetView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      leftTrigger(std::move(other.leftTrigger)),
      rightTrigger(std::move(other.rightTrigger)) {
    setTriggerParent(this, leftTrigger, rightTrigger);
}

void startTriggering(OpSetIntersect& op) {
    op.leftTrigger = make_shared<OpSetIntersectTrigger<true>>(&op);
    op.rightTrigger = make_shared<OpSetIntersectTrigger<false>>(&op);
    addTrigger(op.left, op.leftTrigger);
    addTrigger(op.right, op.rightTrigger);
    startTriggering(op.left);
    startTriggering(op.right);
}

void stopTriggering(OpSetIntersect& op) {
    if (op.leftTrigger) {
        deleteTrigger(op.leftTrigger);
        op.leftTrigger = nullptr;
        stopTriggering(op.left);
    }
    if (op.rightTrigger) {
        deleteTrigger<SetTrigger>(op.rightTrigger);
        op.rightTrigger = nullptr;
        stopTriggering(op.right);
    }
}

void updateViolationDescription(const OpSetIntersect& op,
                                u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.left, parentViolation, vioDesc);
    updateViolationDescription(op.right, parentViolation, vioDesc);
}

std::shared_ptr<OpSetIntersect> deepCopyForUnroll(const OpSetIntersect&,
                                                  const AnyIterRef&) {
    assert(false);
    abort();
}

std::ostream& dumpState(std::ostream& os, const OpSetIntersect&) {
    assert(false);
    return os;
}
