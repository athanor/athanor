
#ifndef SRC_OPERATORS_OPSETFORALL_H_
#define SRC_OPERATORS_OPSETFORALL_H_
#include "operators/opAnd.h"
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"
struct OpSetForAll;
struct OpSetForAll : public BoolView,
                     public Quantifier<SetReturning, BoolReturning, BoolValue> {
    typedef ConjunctionTrigger<OpSetForAll> ExprTrigger;
    typedef ConjunctionIterAssignedTrigger<OpSetForAll> ExprIterAssignedTrigger;
    struct ContainerTrigger;
    struct ContainerIterAssignedTrigger;
    struct DelayedUnrollTrigger;

    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    std::vector<Value> queueOfValuesToAdd;
    std::vector<std::shared_ptr<ExprTrigger>> exprTriggers;
    std::shared_ptr<ContainerTrigger> containerTrigger = nullptr;
    std::shared_ptr<ContainerIterAssignedTrigger> containerIterAssignedTrigger =
        nullptr;
    std::shared_ptr<DelayedUnrollTrigger> delayedUnrollTrigger = nullptr;
    std::vector<Value> valuesToUnroll;
    OpSetForAll(const OpSetForAll&) = delete;
    OpSetForAll(OpSetForAll&& other);
};
#endif /* SRC_OPERATORS_OPSETFORALL_H_ */
