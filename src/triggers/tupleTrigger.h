
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

template <typename Child>
struct ChangeTriggerAdapter<TupleTrigger, Child>
    : public ChangeTriggerAdapterBase<TupleTrigger, Child> {
   private:
    bool wasDefined;
    inline bool eventHandledAsDefinednessChange() {
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

    inline auto& getOp() {
        return static_cast<Child*>(this)->getTriggeringOperand();
    }

   public:
    ChangeTriggerAdapter(const ExprRef<TupleView>& op)
        : wasDefined(op->getViewIfDefined().hasValue()) {}

    inline void memberValueChanged(UInt) final {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    inline void memberHasBecomeDefined(UInt) {
        eventHandledAsDefinednessChange();
    }
    inline void memberHasBecomeUndefined(UInt) {
        eventHandledAsDefinednessChange();
    }
    inline void hasBecomeDefined() { eventHandledAsDefinednessChange(); }
    inline void hasBecomeUndefined() { eventHandledAsDefinednessChange(); }
};
#endif /* SRC_TRIGGERS_TUPLETRIGGER_H_ */
