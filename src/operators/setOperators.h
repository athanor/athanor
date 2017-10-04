#ifndef SRC_OPERATORS_SETOPERATORS_H_
#define SRC_OPERATORS_SETOPERATORS_H_
#include <operators/setProducing.h>
#include <iostream>
#include <memory>
#include <vector>
#include "operators/intProducing.h"
#include "operators/setProducing.h"
#include "types/forwardDecls/hash.h"
struct OpSetIntersect {
    SetProducing left;
    SetProducing right;
    std::unordered_set<u_int64_t> memberHashes;
    u_int64_t cachedHashTotal = 0;
    std::vector<std::shared_ptr<SetTrigger>> triggers;

   private:
    template <bool left>
    class Trigger : public SetTrigger {
        OpSetIntersect& op;

       public:
        Trigger(OpSetIntersect& op) : op(op) {}
        void valueRemoved(const Value& member) final {
            u_int64_t hash = getValueHash(member);
            std::unordered_set<u_int64_t>::iterator hashIter;
            if ((hashIter = op.memberHashes.find(hash)) !=
                op.memberHashes.end()) {
                op.memberHashes.erase(hashIter);
                op.cachedHashTotal -= hash;
                for (auto& trigger : op.triggers) {
                    trigger->valueRemoved(member);
                }
            }
        }

        void valueAdded(const Value& member) final {
            SetProducing& unchanged = (left) ? op.right : op.left;
            u_int64_t hash = getValueHash(member);
            if (op.memberHashes.count(hash)) {
                return;
            }
            SetView viewOfUnchangedSet = getSetView(unchanged);
            if (viewOfUnchangedSet.memberHashes.count(hash)) {
                op.memberHashes.insert(hash);
                op.cachedHashTotal += hash;
                for (auto& trigger : op.triggers) {
                    trigger->valueAdded(member);
                }
            }
        }
    };

   public:
    OpSetIntersect(SetProducing leftIn, SetProducing rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {
        SetView leftSetView = getSetView(left);
        SetView rightSetView = getSetView(right);
        leftSetView.triggers.emplace_back(
            std::make_shared<Trigger<true>>(*this));
        rightSetView.triggers.emplace_back(
            std::make_shared<Trigger<false>>(*this));
    }
};

struct OpSetSize {
    int64_t value = 0;
    SetProducing operand;
    std::vector<std::shared_ptr<IntTrigger>> triggers;

   private:
    class Trigger : public SetTrigger {
        OpSetSize& op;

       public:
        Trigger(OpSetSize& op) : op(op) {}
        void valueRemoved(const Value&) final { --op.value; }

        void valueAdded(const Value&) final { ++op.value; }
    };

   public:
    OpSetSize(SetProducing operandIn) : operand(operandIn) {
        getSetView(operand).triggers.emplace_back(
            std::make_shared<Trigger>(*this));
    }
};
#endif /* SRC_OPERATORS_SETOPERATORS_H_ */
