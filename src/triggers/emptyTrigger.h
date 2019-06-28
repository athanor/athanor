
#ifndef SRC_TRIGGERS_EMPTYTRIGGER_H_
#define SRC_TRIGGERS_EMPTYTRIGGER_H_
#include "base/base.h"
struct EmptyTrigger : public virtual TriggerBase {};

template <>
struct TriggerContainer<EmptyView>
    : public TriggerContainerBase<TriggerContainer<EmptyView> > {
    TriggerQueue<EmptyTrigger> triggers;
    void takeFrom(TriggerContainer<EmptyView>&) { shouldNotBeCalledPanic; }
};

template <typename Child>
struct ChangeTriggerAdapter<EmptyTrigger, Child>
    : public ChangeTriggerAdapterBase<EmptyTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<EmptyView>&) {}
};

template <typename Op, typename Child>
struct ForwardingTrigger<EmptyTrigger, Op, Child>
    : public ForwardingTriggerBase<EmptyTrigger, Op> {
    using ForwardingTriggerBase<EmptyTrigger, Op>::ForwardingTriggerBase;
};

#endif /* SRC_TRIGGERS_EMPTYTRIGGER_H_ */
