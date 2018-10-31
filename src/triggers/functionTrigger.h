
#ifndef SRC_TRIGGERS_FUNCTIONTRIGGER_H_
#define SRC_TRIGGERS_FUNCTIONTRIGGER_H_
#include "base/base.h"
struct FunctionOuterTrigger : public virtual TriggerBase {
    // later will have more specific triggers, currently only those inherited
    // from TriggerBase
};

struct FunctionMemberTrigger : public virtual TriggerBase {
    virtual void imageChanged(UInt index) = 0;

    virtual void imageChanged(const std::vector<UInt>& indices) = 0;
    virtual void imageSwap(UInt index1, UInt index2) = 0;
};

struct FunctionTrigger : public virtual FunctionOuterTrigger,
                         public virtual FunctionMemberTrigger {};
template <typename Child>
struct ChangeTriggerAdapter<FunctionTrigger, Child>
    : public ChangeTriggerAdapterBase<FunctionTrigger, Child> {
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
    ChangeTriggerAdapter(const ExprRef<FunctionView>& op)
        : wasDefined(op->getViewIfDefined().hasValue()) {}
    inline void imageChanged(UInt) final {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    inline void imageChanged(const std::vector<UInt>&) final {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    inline void imageSwap(UInt, UInt) final {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    inline void hasBecomeDefined() { eventHandledAsDefinednessChange(); }
    inline void hasBecomeUndefined() { eventHandledAsDefinednessChange(); }
};

#endif /* SRC_TRIGGERS_FUNCTIONTRIGGER_H_ */
