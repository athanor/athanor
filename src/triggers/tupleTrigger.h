
#ifndef SRC_TRIGGERS_TUPLETRIGGER_H_
#define SRC_TRIGGERS_TUPLETRIGGER_H_
#include "base/base.h"
struct TupleOuterTrigger : public virtual TriggerBase {};

struct TupleMemberTrigger : public virtual TriggerBase {
    virtual void memberValueChanged(UInt index) = 0;

    virtual void memberHasBecomeDefined(UInt index) = 0;
    virtual void memberHasBecomeUndefined(UInt index) = 0;
};

struct TupleTrigger : public virtual TupleOuterTrigger,
                      public virtual TupleMemberTrigger {};

template <>
struct TriggerContainer<TupleView>
    : public TriggerContainerBase<TriggerContainer<TupleView>> {
    TriggerQueue<TupleOuterTrigger> triggers;
    TriggerQueue<TupleMemberTrigger> allMemberTriggers;
    std::vector<TriggerQueue<TupleMemberTrigger>> singleMemberTriggers;

    void takeFrom(TriggerContainer<TupleView>& other) {
        triggers.takeFrom(other.triggers);
        allMemberTriggers.takeFrom(other.allMemberTriggers);
        for (size_t i = 0; i < singleMemberTriggers.size(); i++) {
            if (i >= other.singleMemberTriggers.size()) {
                break;
            }
            singleMemberTriggers[i].takeFrom(other.singleMemberTriggers[i]);
        }
    }

    template <typename Func>
    void visitMemberTriggers(Func&& func, UInt index) {
        visitTriggers(func, allMemberTriggers);
        if (index < singleMemberTriggers.size()) {
            visitTriggers(func, singleMemberTriggers[index]);
        }
    }

    inline void notifyMemberChanged(UInt index) {
        visitMemberTriggers([&](auto& t) { t->memberValueChanged(index); },
                            index);
    }

    inline void notifyMemberDefined(UInt index) {
        visitMemberTriggers([&](auto& t) { t->memberHasBecomeDefined(index); },
                            index);
    }

    inline void notifyMemberUndefined(UInt index) {
        visitMemberTriggers(
            [&](auto& t) { t->memberHasBecomeUndefined(index); }, index);
    }
};

template <typename Child>
struct ChangeTriggerAdapter<TupleTrigger, Child>
    : public ChangeTriggerAdapterBase<TupleTrigger, Child> {
   private:
    bool wasDefined;
    bool eventHandledAsDefinednessChange() {
        bool defined = getOp()->getViewIfDefined().hasValue();
        if (wasDefined && !defined) {
            this->forwardHasBecomeUndefined();
            wasDefined = defined;
            return true;
        } else if (!wasDefined && defined) {
            this->forwardHasBecomeDefined();
            wasDefined = defined;
            return true;
        } else {
            wasDefined = defined;
            return !defined;
        }
    }

    auto& getOp() { return static_cast<Child*>(this)->getTriggeringOperand(); }

   public:
    ChangeTriggerAdapter(const ExprRef<TupleView>& op)
        : wasDefined(op->getViewIfDefined().hasValue()) {}

    void memberValueChanged(UInt) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void memberHasBecomeDefined(UInt) { eventHandledAsDefinednessChange(); }
    void memberHasBecomeUndefined(UInt) { eventHandledAsDefinednessChange(); }
    void hasBecomeDefined() { eventHandledAsDefinednessChange(); }
    void hasBecomeUndefined() { eventHandledAsDefinednessChange(); }
};

template <typename Op, typename Child>
struct ForwardingTrigger<TupleTrigger, Op, Child>
    : public ForwardingTriggerBase<TupleTrigger, Op> {
    using ForwardingTriggerBase<TupleTrigger, Op>::ForwardingTriggerBase;
    void memberValueChanged(UInt index) {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberChanged(index);
        }
    }
    void memberHasBecomeUndefined(UInt index) {
        auto view = static_cast<Child*>(this)
                        ->getTriggeringOperand()
                        ->getViewIfDefined();
        this->op->setAppearsDefined(view.hasValue());
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberUndefined(index);
        }
    }
    void memberHasBecomeDefined(UInt index) {
        auto view = static_cast<Child*>(this)
                        ->getTriggeringOperand()
                        ->getViewIfDefined();
        this->op->setAppearsDefined(view.hasValue());
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberDefined(index);
        }
    }
};

#endif /* SRC_TRIGGERS_TUPLETRIGGER_H_ */
