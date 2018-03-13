
/*#include "operators/opSetIntersect.h"
#include <iostream>
#include <memory>
#include "types/set.h"
#include "base/typeOperations.h"
using namespace std;
void evaluate() {
    left->evaluate();
    right->evaluate();
    silentClear();
    SetView& leftSetView = getView(left);
    SetView& rightSetView = getView(right);
    SetView& smallSetView = (leftSetView.hashIndexMap.size() <
                             rightSetView.hashIndexMap.size())
                                ? leftSetView
                                : rightSetView;
    SetView& largeSetView = (leftSetView.hashIndexMap.size() <
                             rightSetView.hashIndexMap.size())
                                ? rightSetView
                                : leftSetView;
    mpark::visit(
        [&](auto& members) {
            for (auto& hashIndexPair : smallSetView.hashIndexMap) {
                if (largeSetView.hashIndexMap.count(hashIndexPair.first)) {
                    addMember(members[hashIndexPair.second]);
                }
            }
        },
        smallSetView.members);
}

template <bool left>
class OpSetIntersectTrigger : public SetTrigger {
   public:
    OpSetIntersect* op;
    unordered_map<u_int64_t, u_int64_t>::iterator oldHashIter;

   public:
    OpSetIntersectTrigger(OpSetIntersect* op) : op(op) {}
    inline void valueRemoved(u_int64_t, u_int64_t hash) final {
        unordered_map<u_int64_t, u_int64_t>::const_iterator hashIter;
        if ((hashIter = op->hashIndexMap.find(hash)) !=
            op->hashIndexMap.end()) {
            mpark::visit(
                [&](auto& members) {
                    op->removeMemberAndNotify<valType(members)>(
                        hashIter->second);
                },
                op->members);
        }
    }

    inline void valueAdded(const AnyValRef& member) final {
        ExprRef<SetView>& unchanged = (left) ? op->right : op->left;
        u_int64_t hash = getValueHash(member);
        debug_code(assert(!op->hashIndexMap.count(hash)));
        SetView& viewOfUnchangedSet = getView(unchanged);
        if (viewOfUnchangedSet.hashIndexMap.count(hash)) {
            mpark::visit(
                [&](auto& memberImpl) { op->addMemberAndNotify(memberImpl); },
                member);
        }
    }

    inline void setValueChanged(const SetView& newValue) final {
        std::unordered_set<u_int64_t> hashesToRemove;
        for (auto& hashIndexPair : op->hashIndexMap) {
            if (!newValue.hashIndexMap.count(hashIndexPair.first)) {
                hashesToRemove.insert(hashIndexPair.first);
            }
        }
        for (auto& hash : hashesToRemove) {
            this->valueRemoved(0, hash);
            // 0 here is just a dummy value, assuming valueremoved does not use
            // first parameter
        }
        SetView& unchanged = getView((left) ? op->right : op->left);
        mpark::visit(
            [&](auto& newMembersImpl) {

                for (auto& hashIndexPair : newValue.hashIndexMap) {
                    if (!op->hashIndexMap.count(hashIndexPair.first) &&
                        unchanged.hashIndexMap.count(
                            hashIndexPair.first)) {
                        this->valueAdded(newMembersImpl[hashIndexPair.second]);
                    }
                }
            },
            newValue.members);
    }

    inline void possibleMemberValueChange(u_int64_t,
                                          const AnyValRef& member) final {
        u_int64_t hash = getValueHash(member);
        oldHashIter = op->hashIndexMap.find(hash);
        if (oldHashIter != op->hashIndexMap.end()) {
            mpark::visit(
                [&](auto& member) {
                    op->notifyPossibleMemberChange<valType(member)>(
                        oldHashIter->second);
                },
                member);
        }
    }

    inline void memberValueChanged(u_int64_t, const AnyValRef& member) final {
        u_int64_t newHashOfMember = getValueHash(member);
        if (oldHashIter != op->hashIndexMap.end() &&
            newHashOfMember == oldHashIter->first) {
            return;
        }
        ExprRef<SetView>& unchanged = (left) ? op->right : op->left;
        bool containedInUnchangedSet =
            unchanged->hashIndexMap.count(
                newHashOfMember);
        if (oldHashIter != op->hashIndexMap.end()) {
            if (containedInUnchangedSet) {
                mpark::visit(
                    [&](auto& member) {
                        op->memberChangedAndNotify<valType(member)>(
                            oldHashIter->second);
                    },
                    member);
            } else {
                mpark::visit(
                    [&](auto& member) {
                        op->removeMemberAndNotify<valType(member)>(
                            oldHashIter->second);
                    },
                    member);
            }
        } else if (containedInUnchangedSet) {
            this->valueAdded(member);
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

void startTriggering() {
if (!leftTrigger {
    leftTrigger = make_shared<OpSetIntersectTrigger<true>>(this);
    rightTrigger = make_shared<OpSetIntersectTrigger<false>>(this);
    addTrigger(left, leftTrigger);
    addTrigger(right, rightTrigger);
    left->startTriggering();
    right->startTriggering();
    }
}

void stopTriggering() {
    if (leftTrigger) {
        deleteTrigger(leftTrigger);
        leftTrigger = nullptr;
        left->stopTriggering();
    }
    if (rightTrigger) {
        deleteTrigger<SetTrigger>(rightTrigger);
        rightTrigger = nullptr;
        right->stopTriggering();
    }
}

void updateViolationDescription(
                                u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(left, parentViolation, vioDesc);
    updateViolationDescription(right, parentViolation, vioDesc);
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

*/
