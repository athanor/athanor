

#ifndef SRC_TRIGGERS_PARTITIONTRIGGER_H_
#define SRC_TRIGGERS_PARTITIONTRIGGER_H_
#include "base/base.h"
struct PartitionOuterTrigger : public virtual TriggerBase {};

struct PartitionMemberTrigger : public virtual TriggerBase {
    virtual void containingPartsSwapped(UInt member1, UInt member2) = 0;
    virtual void membersMovedToPart(const std::vector<UInt>& memberIndices,
                                    const std::vector<UInt>& oldParts,
                                    UInt destPart) = 0;
    virtual void membersMovedFromPart(UInt part,
                                      const std::vector<UInt>& memberIndices,
                                      const std::vector<UInt>& newParts) = 0;
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

    inline void notifyMembersMovedToPart(const std::vector<UInt>& memberIndices,
                                         const std::vector<UInt>& oldParts,
                                         UInt destPart) {
        auto triggerFunc = [&](auto& t) {
            t->membersMovedToPart(memberIndices, oldParts, destPart);
        };
        visitTriggers(triggerFunc, allMemberTriggers);
        for (auto& member : memberIndices) {
            if (member < singleMemberTriggers.size()) {
                visitTriggers(triggerFunc, singleMemberTriggers[member]);
            }
        }
    }

    inline void notifyMembersMovedFromPart(
        UInt part, const std::vector<UInt>& memberIndices,
        const std::vector<UInt>& newParts) {
        auto triggerFunc = [&](auto& t) {
            t->membersMovedFromPart(part, memberIndices, newParts);
        };
        visitTriggers(triggerFunc, allMemberTriggers);
        for (auto index : memberIndices) {
            if (index < singleMemberTriggers.size()) {
                visitTriggers(triggerFunc, singleMemberTriggers[index]);
            }
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
    void membersMovedToPart(const std::vector<UInt>&, const std::vector<UInt>&,
                            UInt) override {
        this->forwardValueChanged();
    }
    void membersMovedFromPart(UInt, const std::vector<UInt>&,
                              const std::vector<UInt>&) override {
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
    void membersMovedToPart(const std::vector<UInt>& memberIndices,
                            const std::vector<UInt>& oldParts,
                            UInt destPart) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMembersMovedToPart(memberIndices, oldParts,
                                               destPart);
        }
    }

    void membersMovedFromPart(UInt part, const std::vector<UInt>& memberIndices,
                              const std::vector<UInt>& newParts) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMembersMovedFromPart(part, memberIndices, newParts);
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
