
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
template <typename Child>
struct ChangeTriggerAdapter<SetTrigger, Child>
    : public ChangeTriggerAdapterBase<SetTrigger, Child> {
    inline void valueRemoved(UInt, HashType) { this->forwardValueChanged(); }
    inline void valueAdded(const AnyExprRef&) final {
        this->forwardValueChanged();
    }

    inline void memberValueChanged(UInt, HashType) final {
        this->forwardValueChanged();
    }

    inline void memberValuesChanged(const std::vector<UInt>&,
                                    const std::vector<HashType>&) final {
        this->forwardValueChanged();
    }
};

#endif /* SRC_TRIGGERS_SETTRIGGER_H_ */
