
#ifndef SRC_TRIGGERS_MSETTRIGGER_H_
#define SRC_TRIGGERS_MSETTRIGGER_H_
#include "base/base.h"
struct MSetTrigger : public virtual TriggerBase {
    virtual void valueRemoved(UInt indexOfRemovedValue,
                              const AnyExprRef& removedExpr) = 0;
    virtual void valueAdded(const AnyExprRef& member) = 0;
    virtual void memberValueChanged(UInt index) = 0;
    virtual void memberValuesChanged(const std::vector<UInt>& indices) = 0;
};

struct TriggerContainer<MSetView> {
    std::vector<std::shared_ptr<MSetTrigger>> triggers;
    inline void notifyMemberAdded(const AnyExprRef& newMember) {
        visitTriggers([&](auto& t) { t->valueAdded(newMember); }, triggers);
    }

    inline void notifyMemberRemoved(UInt index, const AnyExprRef& expr) {
        visitTriggers([&](auto& t) { t->valueRemoved(index, expr); }, triggers);
    }


    inline void notifyMemberChanged(size_t index) {
        visitTriggers([&](auto& t) { t->memberValueChanged(index); }, triggers);
    }


    inline void notifyMembersChanged(const std::vector<UInt>& indices) {
        visitTriggers([&](auto& t) { t->memberValuesChanged(indices); },
                      triggers);
    }
};

template <typename Child>
struct ChangeTriggerAdapter<MSetTrigger, Child>
    : public ChangeTriggerAdapterBase<MSetTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<MSetView>&) {}
    void valueRemoved(UInt, const AnyExprRef&) { this->forwardValueChanged(); }
    void valueAdded(const AnyExprRef&) { this->forwardValueChanged(); }

    void memberValueChanged(UInt) { this->forwardValueChanged(); }

    void memberValuesChanged(const std::vector<UInt>&) {
        this->forwardValueChanged();
    }
};

template <typename Op>
struct ForwardingTrigger<MSetTrigger, Op>
    : public ForwardingTriggerBase<MSetTrigger, Op> {
    using ForwardingTriggerBase<MSetTrigger, Op>::ForwardingTriggerBase;
    void valueRemoved(UInt index, const AnyExprRef& oldVal) {
        visitTriggers([&](auto& t) { t->valueRemoved(index, oldVal); },
                      this->op->triggers);
    }
    void valueAdded(const AnyExprRef& expr) {
        visitTriggers([&](auto& t) { t->valueAdded(expr); },
                      this->op->triggers);
    }

    void memberValueChanged(UInt index, HashType oldHash) {
        visitTriggers([&](auto& t) { t->memberValueChanged(index, oldHash); },
                      this->op->triggers);
    }

    void memberValuesChanged(const std::vector<UInt>& indices,
                             const std::vector<HashType>& oldHashes) {
        visitTriggers(
            [&](auto& t) { t->memberValuesChanged(indices, oldHashes); },
            this->op->triggers);
    }
};

#endif /* SRC_TRIGGERS_MSETTRIGGER_H_ */
