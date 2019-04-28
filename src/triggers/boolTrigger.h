
#ifndef SRC_TRIGGERS_BOOLTRIGGER_H_
#define SRC_TRIGGERS_BOOLTRIGGER_H_
#include "base/base.h"

template <>
struct TriggerContainer<BoolView>
    : public TriggerContainerBase<TriggerContainer<BoolView>> {
    TriggerQueue<BoolTrigger> triggers;

    void takeFrom(TriggerContainer<BoolView>& other) {
        triggers.takeFrom(other.triggers);
    }
};
template <typename Child>
struct ChangeTriggerAdapter<BoolTrigger, Child>
    : public ChangeTriggerAdapterBase<BoolTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<BoolView>&) {}
};
template <typename Op, typename Child>
struct ForwardingTrigger<BoolTrigger, Op, Child>
    : public ForwardingTriggerBase<BoolTrigger, Op> {
    using ForwardingTriggerBase<BoolTrigger, Op>::ForwardingTriggerBase;
};

#endif /* SRC_TRIGGERS_BOOLTRIGGER_H_ */
