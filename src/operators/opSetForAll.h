
#ifndef SRC_OPERATORS_OPSETFORALL_H_
#define SRC_OPERATORS_OPSETFORALL_H_
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
struct OpSetForAll : public BoolView,
                     public Quantifier<SetReturning, BoolReturning, BoolValue> {
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
};
#endif /* SRC_OPERATORS_OPSETFORALL_H_ */
