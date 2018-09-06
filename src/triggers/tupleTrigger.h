
#ifndef SRC_TRIGGERS_TUPLETRIGGER_H_
#define SRC_TRIGGERS_TUPLETRIGGER_H_
#include "base/base.h"
struct TupleTrigger : public virtual TriggerBase {
    virtual void memberValueChanged(UInt index) = 0;
    virtual void memberHasBecomeUndefined(UInt) = 0;
    virtual void memberHasBecomeDefined(UInt) = 0;
};
template <typename Child>
struct ChangeTriggerAdapter<TupleTrigger, Child>
    : public ChangeTriggerAdapterBase<TupleTrigger, Child> {
    inline void memberValueChanged(UInt) final { this->forwardValueChanged(); }
    inline void memberHasBecomeDefined(UInt) {}
    inline void memberHasBecomeUndefined(UInt) {}
};

#endif /* SRC_TRIGGERS_TUPLETRIGGER_H_ */
