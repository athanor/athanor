
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
    inline void imageChanged(UInt) final { this->forwardValueChanged(); }
    inline void imageChanged(const std::vector<UInt>&) final {
        this->forwardValueChanged();
    }
    inline void imageSwap(UInt, UInt) final { this->forwardValueChanged(); }
};

#endif /* SRC_TRIGGERS_FUNCTIONTRIGGER_H_ */
