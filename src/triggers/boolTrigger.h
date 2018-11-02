
#ifndef SRC_TRIGGERS_BOOLTRIGGER_H_
#define SRC_TRIGGERS_BOOLTRIGGER_H_
#include "base/base.h"
template <typename Child>
struct ChangeTriggerAdapter<BoolTrigger, Child>
    : public ChangeTriggerAdapterBase<BoolTrigger, Child> {
    ChangeTriggerAdapter(const ExprRef<BoolView>&) {}
};
template <typename Op>
struct ForwardingTrigger<BoolTrigger, Op>
    : public ForwardingTriggerBase<BoolTrigger, Op> {
    using ForwardingTriggerBase<BoolTrigger, Op>::ForwardingTriggerBase;
};

#endif /* SRC_TRIGGERS_BOOLTRIGGER_H_ */
