
#ifndef SRC_TRIGGERS_ENUMTRIGGER_H_
#define SRC_TRIGGERS_ENUMTRIGGER_H_
#include "base/base.h"
struct EnumTrigger : public virtual TriggerBase {
    inline void memberReplaced(UInt, const AnyExprRef&) override {
        shouldNotBeCalledPanic;
    }
};

template <>
struct TriggerContainer<EnumView>
    : public TriggerContainerBase<TriggerContainer<EnumView> > {
    TriggerQueue<EnumTrigger> triggers;

    void takeFrom(TriggerContainer<EnumView>& other) {
        triggers.takeFrom(other.triggers);
    }
};
template <typename Child>
struct ChangeTriggerAdapter<EnumTrigger, Child>
    : public ChangeTriggerAdapterBase<EnumTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<EnumView>&) {}
};
template <typename Op, typename Child>
struct ForwardingTrigger<EnumTrigger, Op, Child>
    : public ForwardingTriggerBase<EnumTrigger, Op> {
    using ForwardingTriggerBase<EnumTrigger, Op>::ForwardingTriggerBase;
};
#endif /* SRC_TRIGGERS_ENUMTRIGGER_H_ */
