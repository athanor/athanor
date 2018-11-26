
#ifndef SRC_TRIGGERS_INTTRIGGER_H_
#define SRC_TRIGGERS_INTTRIGGER_H_
#include "base/base.h"
struct IntTrigger : public virtual TriggerBase {};

template <>
struct TriggerContainer<IntView> {
    TriggerQueue<IntTrigger> triggers;
};
template <typename Child>
struct ChangeTriggerAdapter<IntTrigger, Child>
    : public ChangeTriggerAdapterBase<IntTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<IntView>&) {}
};
template <typename Op, typename Child>
struct ForwardingTrigger<IntTrigger, Op, Child>
    : public ForwardingTriggerBase<IntTrigger, Op> {
    using ForwardingTriggerBase<IntTrigger, Op>::ForwardingTriggerBase;
};
#endif /* SRC_TRIGGERS_INTTRIGGER_H_ */
