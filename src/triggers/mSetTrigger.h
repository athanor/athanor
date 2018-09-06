
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

template <typename Child>
struct ChangeTriggerAdapter<MSetTrigger, Child>
    : public ChangeTriggerAdapterBase<MSetTrigger, Child> {
    inline void valueRemoved(UInt, const AnyExprRef&) {
        this->forwardValueChanged();
    }
    inline void valueAdded(const AnyExprRef&) final {
        this->forwardValueChanged();
    }

    inline void memberValueChanged(UInt) final { this->forwardValueChanged(); }

    inline void memberValuesChanged(const std::vector<UInt>&) final {
        this->forwardValueChanged();
    }
};

#endif /* SRC_TRIGGERS_MSETTRIGGER_H_ */
