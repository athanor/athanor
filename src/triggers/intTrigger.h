
#ifndef SRC_TRIGGERS_INTTRIGGER_H_
#define SRC_TRIGGERS_INTTRIGGER_H_
#include "base/base.h"
struct IntTrigger : public virtual TriggerBase {};

template <typename Child>
struct ChangeTriggerAdapter<IntTrigger, Child>
    : public ChangeTriggerAdapterBase<IntTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<IntView>&) {}
};

#endif /* SRC_TRIGGERS_INTTRIGGER_H_ */
