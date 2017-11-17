
#ifndef SRC_OPERATORS_OPSETFORALL_H_
#define SRC_OPERATORS_OPSETFORALL_H_
#include "operators/opAnd.h"
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"
struct OpSetForAll;
typedef ConjunctionTrigger<OpSetForAll> OpSetForAllExprTrigger;
typedef ConjunctionIterAssignedTrigger<OpSetForAll>
    OpSetForAllExprIterAssignedTrigger;
class OpSetForAllContainerTrigger;
class OpSetForAllContainerIterAssignedTrigger;
class OpSetForAllContainerDelayedTrigger;
struct OpSetForAll : public BoolView,
                     public Quantifier<SetReturning, BoolReturning, BoolValue> {
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    std::vector<Value> queueOfValuesToAdd;
    std::vector<std::shared_ptr<OpSetForAllExprTrigger>> exprTriggers;
    std::shared_ptr<OpSetForAllContainerTrigger> containerTrigger = nullptr;
    std::shared_ptr<OpSetForAllContainerIterAssignedTrigger>
        containerIterAssignedTrigger = nullptr;
    std::shared_ptr<OpSetForAllContainerDelayedTrigger>
        containerDelayedTrigger = nullptr;

    OpSetForAll(const OpSetForAll&) = delete;
    OpSetForAll(OpSetForAll&& other);
};
#endif /* SRC_OPERATORS_OPSETFORALL_H_ */
