#ifndef SRC_TYPES_EMPTY_H_
#define SRC_TYPES_EMPTY_H_
#include "base/base.h"
#include "common/common.h"
#include "triggers/emptyTrigger.h"
struct EmptyView : public ExprInterface<EmptyView>,
                   public TriggerContainer<EmptyView> {};

#endif /* SRC_TYPES_EMPTY_H_ */
