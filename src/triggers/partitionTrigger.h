

#ifndef SRC_TRIGGERS_PARTITIONTRIGGER_H_
#define SRC_TRIGGERS_PARTITIONTRIGGER_H_
#include "base/base.h"
struct PartitionOuterTrigger : public virtual TriggerBase {
};

struct PartitionMemberTrigger : public virtual TriggerBase {
    virtual void containingPartsSwapped(UInt member1, UInt member2) = 0;
};

struct PartitionTrigger : public virtual PartitionOuterTrigger,
                         public virtual PartitionMemberTrigger {};

template <>
struct TriggerContainer<PartitionView> {
    TriggerQueue<PartitionOuterTrigger> triggers;
    TriggerQueue<PartitionMemberTrigger> allMemberTriggers;
    std::vector<TriggerQueue<PartitionMemberTrigger>> singleMemberTriggers;




    inline void notifyContainingPartsSwapped(UInt member1, UInt member2) {
        auto triggerFunc = [&] (auto& t) {
            t->containingPartsSwapped(member1,member2);
        };
        visitTriggers(triggerFunc,allMemberTriggers);
        if (member1 < singleMemberTriggers.size()) {
            visitTriggers(triggerFunc,singleMemberTriggers[member1]);
        }
        if (member2 < singleMemberTriggers.size()) {
            visitTriggers(triggerFunc,singleMemberTriggers[member2]);
        }
    }
};

template <typename Child>
struct ChangeTriggerAdapter<PartitionTrigger, Child>
    : public ChangeTriggerAdapterBase<PartitionTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<PartitionView>&)  {}
    void containingPartsSwapped(UInt, UInt) override {
            this->forwardValueChanged();
    }
};



template <typename Op, typename Child>
struct ForwardingTrigger<PartitionTrigger, Op, Child>
    : public ForwardingTriggerBase<PartitionTrigger, Op> {
    using ForwardingTriggerBase<PartitionTrigger, Op>::ForwardingTriggerBase;

   public:

        void containingTriggersSwapped(UInt member1,UInt member2) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyContainingPartsSwapped(member1, member2);
        }
    }
};

#endif /* SRC_TRIGGERS_PARTITIONTRIGGER_H_ */
