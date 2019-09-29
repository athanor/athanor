

#ifndef SRC_TRIGGERS_PARTITIONTRIGGER_H_
#define SRC_TRIGGERS_PARTITIONTRIGGER_H_
#include "base/base.h"
struct PartitionOuterTrigger : public virtual TriggerBase {};

struct PartitionMemberTrigger : public virtual TriggerBase {
    virtual void containingPartsSwapped(UInt member1, UInt member2) = 0;
};

struct PartitionTrigger : public virtual PartitionOuterTrigger,
                          public virtual PartitionMemberTrigger {};

template <>
struct TriggerContainer<PartitionView>
    : public TriggerContainerBase<TriggerContainer<PartitionView>> {
    TriggerQueue<PartitionOuterTrigger> triggers;
    TriggerQueue<PartitionMemberTrigger> allMemberTriggers;
    std::vector<TriggerQueue<PartitionMemberTrigger>> singleMemberTriggers;

    void takeFrom(TriggerContainer<PartitionView>& other) {
        triggers.takeFrom(other.triggers);
        allMemberTriggers.takeFrom(other.allMemberTriggers);
        for (size_t i = 0; i < singleMemberTriggers.size(); i++) {
            if (i >= other.singleMemberTriggers.size()) {
                break;
            }
            singleMemberTriggers[i].takeFrom(other.singleMemberTriggers[i]);
        }
    }

    void notifyMemberReplaced(UInt index, const AnyExprRef& oldMember) {
        auto triggerFunc = [&](auto& t) {
            t->memberReplaced(index, oldMember);
        };
        visitTriggers(triggerFunc, allMemberTriggers);
        if (index < singleMemberTriggers.size()) {
            visitTriggers(triggerFunc, singleMemberTriggers[index]);
        }
    }

    inline void notifyContainingPartsSwapped(UInt member1, UInt member2) {
        auto triggerFunc = [&](auto& t) {
            t->containingPartsSwapped(member1, member2);
        };
        visitTriggers(triggerFunc, allMemberTriggers);
        if (member1 < singleMemberTriggers.size()) {
            visitTriggers(triggerFunc, singleMemberTriggers[member1]);
        }
        if (member2 < singleMemberTriggers.size()) {
            visitTriggers(triggerFunc, singleMemberTriggers[member2]);
        }
    }
};

template <typename Child>
struct ChangeTriggerAdapter<PartitionTrigger, Child>
    : public ChangeTriggerAdapterBase<PartitionTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<PartitionView>&) {}
    void containingPartsSwapped(UInt, UInt) override {
        this->forwardValueChanged();
    }
    inline void memberReplaced(UInt, const AnyExprRef&) override {
        this->forwardValueChanged();
    }
};

template <typename Op, typename Child>
struct ForwardingTrigger<PartitionTrigger, Op, Child>
    : public ForwardingTriggerBase<PartitionTrigger, Op> {
    using ForwardingTriggerBase<PartitionTrigger, Op>::ForwardingTriggerBase;

   public:
    void containingPartsSwapped(UInt member1, UInt member2) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyContainingPartsSwapped(member1, member2);
        }
    }
    inline void memberReplaced(UInt index,
                               const AnyExprRef& oldMember) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberReplaced(index, oldMember);
        }
    }
};

#endif /* SRC_TRIGGERS_PARTITIONTRIGGER_H_ */
