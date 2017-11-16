
#ifndef SRC_OPERATORS_OPSETFORALL_H_
#define SRC_OPERATORS_OPSETFORALL_H_
#include "operators/opAnd.h"
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"
struct OpSetForAll;
typedef ConjunctionTrigger<OpSetForAll> OpSetForAllExprTrigger;
typedef ConjunctionIterValueChangeTrigger<OpSetForAll>
    OpSetForAllExprIterValueChangeTrigger;

struct OpSetForAll : public BoolView,
                     public Quantifier<SetReturning, BoolReturning, BoolValue> {
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    std::vector<std::shared_ptr<OpSetForAllExprTrigger>> exprTriggers;
};
#endif /* SRC_OPERATORS_OPSETFORALL_H_ */
