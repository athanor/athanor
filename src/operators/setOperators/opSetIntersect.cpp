#include "operators/setOperators.h"
using namespace std;

void evaluate(OpSetIntersect& op) {
    evaluate(op.left);
    evaluate(op.right);
    op.cachedHashTotal = 0;
    op.memberHashes.clear();
    SetView& leftSetView = getSetView(op.left);
    SetView& rightSetView = getSetView(op.right);
    SetView& smallSetView =
        (leftSetView.memberHashes.size() < rightSetView.memberHashes.size())
            ? leftSetView
            : rightSetView;
    SetView& largeSetView =
        (leftSetView.memberHashes.size() < rightSetView.memberHashes.size())
            ? rightSetView
            : leftSetView;
    for (u_int64_t hash : smallSetView.memberHashes) {
        if (largeSetView.memberHashes.count(hash)) {
            op.memberHashes.insert(hash);
            op.cachedHashTotal += hash;
        }
    }
}

template <bool left>
class OpSetIntersectTrigger : public SetTrigger {
    OpSetIntersect& op;
    unordered_set<u_int64_t>::iterator oldHashIter;

   public:
    OpSetIntersectTrigger(OpSetIntersect& op) : op(op) {}
    inline void valueRemoved(const Value& member) final {
        u_int64_t hash = mix(getValueHash(member));
        unordered_set<u_int64_t>::iterator hashIter;
        if ((hashIter = op.memberHashes.find(hash)) != op.memberHashes.end()) {
            op.memberHashes.erase(hashIter);
            op.cachedHashTotal -= hash;
            for (auto& trigger : op.triggers) {
                trigger->valueRemoved(member);
            }
        }
    }

    inline void valueAdded(const Value& member) final {
        SetProducing& unchanged = (left) ? op.right : op.left;
        u_int64_t hash = mix(getValueHash(member));
        if (op.memberHashes.count(hash)) {
            return;
        }
        SetView& viewOfUnchangedSet = getSetView(unchanged);
        if (viewOfUnchangedSet.memberHashes.count(hash)) {
            op.memberHashes.insert(hash);
            op.cachedHashTotal += hash;
            for (auto& trigger : op.triggers) {
                trigger->valueAdded(member);
            }
        }
    }
    inline void possibleValueChange(const Value& member) final {
        u_int64_t hash = mix(getValueHash(member));
        oldHashIter = op.memberHashes.find(hash);
        if (oldHashIter != op.memberHashes.end()) {
            for (auto& trigger : op.triggers) {
                trigger->possibleValueChange(member);
            }
        }
    }

    inline void valueChanged(const Value& member) final {
        u_int64_t newHashOfMember = mix(getValueHash(member));
        if (oldHashIter != op.memberHashes.end() &&
            newHashOfMember == *oldHashIter) {
            return;
        }
        SetProducing& unchanged = (left) ? op.right : op.left;
        bool containedInUnchangedSet =
            getSetView(unchanged).memberHashes.count(newHashOfMember);
        if (oldHashIter != op.memberHashes.end()) {
            op.cachedHashTotal -= *oldHashIter;
            op.memberHashes.erase(oldHashIter);
            if (containedInUnchangedSet) {
                op.memberHashes.insert(newHashOfMember);
                op.cachedHashTotal += newHashOfMember;
                for (auto& trigger : op.triggers) {
                    trigger->valueChanged(member);
                }
            } else {
                for (auto& trigger : op.triggers) {
                    trigger->valueRemoved(member);
                }
            }
        } else if (containedInUnchangedSet) {
            op.memberHashes.insert(newHashOfMember);
            op.cachedHashTotal += newHashOfMember;
            for (auto& trigger : op.triggers) {
                trigger->valueAdded(member);
            }
        }
    }
};

void startTriggering(OpSetIntersect& op) {
    getSetView(op.left).triggers.emplace_back(
        make_shared<OpSetIntersectTrigger<true>>(op));
    getSetView(op.right).triggers.emplace_back(
        make_shared<OpSetIntersectTrigger<false>>(op));
    startTriggering(op.left);
    startTriggering(op.right);
}

void stopTriggering(OpSetIntersect& op) {
    assert(false);  // todo
    stopTriggering(op.left);
    stopTriggering(op.right);
}

void updateViolationDescription(const OpSetIntersect& op,
                                u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.left, parentViolation, vioDesc);
    updateViolationDescription(op.right, parentViolation, vioDesc);
}
