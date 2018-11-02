
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

struct TriggerContainer<TupleView> {
    std::vector<std::shared_ptr<TupleOuterTrigger>> triggers;
    std::vector<std::shared_ptr<TupleMemberTrigger>> allMemberTriggers;
    std::vector<std::vector<std::shared_ptr<TupleMemberTrigger>>>
        singleMemberTriggers;
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

template <typename Op>
struct ForwardingTrigger<TupleTrigger, Op>
    : public ForwardingTriggerBase<TupleTrigger, Op> {
    using ForwardingTriggerBase<TupleTrigger, Op>::ForwardingTriggerBase;
    void memberValueChanged(UInt index) {
        visitTriggers([&](auto& t) { t->memberValueChanged(index); },
                      this->op->triggers);
    }
    void memberHasBecomeUndefined(UInt index) {
        visitTriggers([&](auto& t) { t->memberHasBecomeUndefined(index); },
                      this->op->triggers);
    }
    void memberHasBecomeDefined(UInt index) {
        visitTriggers([&](auto& t) { t->memberHasBecomeDefined(index); },
                      this->op->triggers);
    }
};

#endif /* SRC_TRIGGERS_TUPLETRIGGER_H_ */
