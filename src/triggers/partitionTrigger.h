

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


    template <typename Func>
    void visitMemberTriggers(Func&& func, UInt index) {
        visitTriggers(func, allMemberTriggers);
        if (index < singleMemberTriggers.size()) {
            visitTriggers(func, singleMemberTriggers[index]);
        }
    }

    inline void notifyContainingPartsSwapped(UInt member1, UInt member2) {
        visitMemberTriggers([&] (auto& t) {
            t->containingPartsSwapped(member1,member2);
        });
    }
};

template <typename Child>
struct ChangeTriggerAdapter<PartitionTrigger, Child>
    : public ChangeTriggerAdapterBase<PartitionTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<PartitionView>&)  {}
    void containingPartsSwapped(UInt member1, UInt member2) override {
            this->forwardValueChanged();
    }
};



template <typename Op, typename Child>
struct ForwardingTrigger<PartitionTrigger, Op, Child>
    : public ForwardingTriggerBase<PartitionTrigger, Op> {
    using ForwardingTriggerBase<PartitionTrigger, Op>::ForwardingTriggerBase;

   public:

        void positionsSwapped(HashType index1, UInt index2) {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyPositionsSwapped(index1, index2);
        }
    }
};

#endif /* SRC_TRIGGERS_PARTITIONTRIGGER_H_ */
