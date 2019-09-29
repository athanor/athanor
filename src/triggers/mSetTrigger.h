
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

template <>
struct TriggerContainer<MSetView>
    : public TriggerContainerBase<TriggerContainer<MSetView>> {
    TriggerQueue<MSetTrigger> triggers;

    void takeFrom(TriggerContainer<MSetView>& other) {
        triggers.takeFrom(other.triggers);
    }

    inline void notifyMemberAdded(const AnyExprRef& newMember) {
        visitTriggers([&](auto& t) { t->valueAdded(newMember); }, triggers);
    }

    inline void notifyMemberRemoved(UInt index, const AnyExprRef& expr) {
        visitTriggers([&](auto& t) { t->valueRemoved(index, expr); }, triggers);
    }

    void notifyMemberReplaced(UInt index, const AnyExprRef& oldMember) {
        visitTriggers([&](auto& t) { t->memberReplaced(index, oldMember); },
                      triggers);
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
    void valueRemoved(UInt, const AnyExprRef&) override {
        this->forwardValueChanged();
    }
    void valueAdded(const AnyExprRef&) override { this->forwardValueChanged(); }

    void memberValueChanged(UInt) override { this->forwardValueChanged(); }

    inline void memberReplaced(UInt, const AnyExprRef&) override {
        this->forwardValueChanged();
    }
    void memberValuesChanged(const std::vector<UInt>&) override {
        this->forwardValueChanged();
    }
};

template <typename Op, typename Child>
struct ForwardingTrigger<MSetTrigger, Op, Child>
    : public ForwardingTriggerBase<MSetTrigger, Op> {
    using ForwardingTriggerBase<MSetTrigger, Op>::ForwardingTriggerBase;
    void valueRemoved(UInt index, const AnyExprRef& expr) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberRemoved(index, expr);
        }
    }
    void valueAdded(const AnyExprRef& expr) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberAdded(expr);
        }
    }

    void memberValueChanged(UInt index) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberChanged(index);
        }
    }

    void memberReplaced(UInt index, const AnyExprRef& oldMember) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberReplaced(index, oldMember);
        }
    }

    void memberValuesChanged(const std::vector<UInt>& indices) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMembersChanged(indices);
        }
    }
};

#endif /* SRC_TRIGGERS_MSETTRIGGER_H_ */
