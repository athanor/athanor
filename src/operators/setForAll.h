
#ifndef SRC_OPERATORS_SETFORALL_H_
#define SRC_OPERATORS_SETFORALL_H_
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
struct SetForAll
    : public Quantifier<SetReturning, BoolReturning, BoolValue, BoolView> {
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
};
#endif /* SRC_OPERATORS_SETFORALL_H_ */
