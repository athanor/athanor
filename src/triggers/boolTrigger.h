
#ifndef SRC_TRIGGERS_BOOLTRIGGER_H_
#define SRC_TRIGGERS_BOOLTRIGGER_H_
#include "base/base.h"
template <typename Child>
struct ChangeTriggerAdapter<BoolTrigger, Child>
    : public ChangeTriggerAdapterBase<BoolTrigger, Child> {};

#endif /* SRC_TRIGGERS_BOOLTRIGGER_H_ */