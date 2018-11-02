
#ifndef SRC_TRIGGERS_SETTRIGGER_H_
#define SRC_TRIGGERS_SETTRIGGER_H_
#include "base/base.h"
struct SetTrigger : public virtual TriggerBase {
    virtual void valueRemoved(UInt indexOfRemovedValue,
                              HashType hashOfRemovedValue) = 0;
    virtual void valueAdded(const AnyExprRef& member) = 0;
    virtual void memberValueChanged(UInt index, HashType oldHash) = 0;

    virtual void memberValuesChanged(
        const std::vector<UInt>& indices,
        const std::vector<HashType>& oldHashes) = 0;
};

template <>
struct TriggerContainer<SetView> {
    std::vector<std::shared_ptr<SetTrigger>> triggers;
    inline void notifyMemberAdded(const AnyExprRef& newMember) {
        visitTriggers([&](auto& t) { t->valueAdded(newMember); }, triggers);
    }

    inline void notifyMemberRemoved(UInt index, HashType hashOfRemovedMember) {
        visitTriggers(
            [&](auto& t) { t->valueRemoved(index, hashOfRemovedMember); },
            triggers);
    }
    inline void notifyMemberChanged(size_t index, HashType oldHash) {
        visitTriggers([&](auto& t) { t->memberValueChanged(index, oldHash); },
                      triggers);
    }
    inline void notifyMembersChanged(const std::vector<UInt>& indices,
                                     const std::vector<HashType>& oldHashes) {
        visitTriggers(
            [&](auto& t) { t->memberValuesChanged(indices, oldHashes); },
            triggers);
    }
};

template <typename Child>
struct ChangeTriggerAdapter<SetTrigger, Child>
    : public ChangeTriggerAdapterBase<SetTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<SetView>&) {}
    void valueRemoved(UInt, HashType) { this->forwardValueChanged(); }
    void valueAdded(const AnyExprRef&) { this->forwardValueChanged(); }

    void memberValueChanged(UInt, HashType) { this->forwardValueChanged(); }

    void memberValuesChanged(const std::vector<UInt>&,
                             const std::vector<HashType>&) {
        this->forwardValueChanged();
    }
};

template <typename Op, typename Child>
struct ForwardingTrigger<SetTrigger, Op, Child>
    : public ForwardingTriggerBase<SetTrigger, Op> {
    using ForwardingTriggerBase<SetTrigger, Op>::ForwardingTriggerBase;
    void valueRemoved(UInt index, HashType oldHash) {
        this->op->notifyMemberRemoved(index, oldHash);
    }
    void valueAdded(const AnyExprRef& expr) {
        this->op->notifyMemberAdded(expr);
    }

    void memberValueChanged(UInt index, HashType oldHash) {
        this->op->notifyMemberChanged(index, oldHash);
    }

    void memberValuesChanged(const std::vector<UInt>& indices,
                             const std::vector<HashType>& oldHashes) {
        this->op->notifyMembersChanged(indices, oldHashes);
    }
};

#endif /* SRC_TRIGGERS_SETTRIGGER_H_ */
