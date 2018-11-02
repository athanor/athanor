
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

struct TriggerContainer<FunctionView> {
    std::vector<std::shared_ptr<FunctionOuterTrigger>> triggers;
    std::vector<std::shared_ptr<FunctionMemberTrigger>> allMemberTriggers;
    std::vector<std::vector<std::shared_ptr<FunctionMemberTrigger>>>
        singleMemberTriggers;
};

template <typename Child>
struct ChangeTriggerAdapter<FunctionTrigger, Child>
    : public ChangeTriggerAdapterBase<FunctionTrigger, Child> {
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
    ChangeTriggerAdapter(const ExprRef<FunctionView>& op)
        : wasDefined(op->getViewIfDefined().hasValue()) {}
    void imageChanged(UInt) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void imageChanged(const std::vector<UInt>&) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void imageSwap(UInt, UInt) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void hasBecomeDefined() { eventHandledAsDefinednessChange(); }
    void hasBecomeUndefined() { eventHandledAsDefinednessChange(); }
};
template <typename Op>
struct ForwardingTrigger<FunctionTrigger, Op>
    : public ForwardingTriggerBase<FunctionTrigger, Op> {
    using ForwardingTriggerBase<FunctionTrigger, Op>::ForwardingTriggerBase;
    void imageChanged(const std::vector<UInt>& indices) {
        visitTriggers([&](auto& t) { t->imageChanged(indices); },
                      this->op->triggers);
    }
    void imageChanged(UInt index) {
        visitTriggers([&](auto& t) { t->imageChanged(index); },
                      this->op->triggers);
    }

    void imageSwap(UInt index1, UInt index2) {
        visitTriggers([&](auto& t) { t->imageSwap(index1, index2); },
                      this->op->triggers);
    }
};

#endif /* SRC_TRIGGERS_FUNCTIONTRIGGER_H_ */
