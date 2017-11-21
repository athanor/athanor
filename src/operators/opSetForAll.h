
#ifndef SRC_OPERATORS_OPSETFORALL_H_
#define SRC_OPERATORS_OPSETFORALL_H_
#include "operators/opAnd.h"
#include "operators/operatorBase.h"
#include "operators/quantifier.h"
#include "types/bool.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"
struct OpSetForAll;
struct OpSetForAll
    : public BoolView,
      public Quantifier<SetReturning, SetValue, BoolReturning, BoolValue> {
    typedef ConjunctionTrigger<OpSetForAll> ExprTrigger;
    typedef ConjunctionIterAssignedTrigger<OpSetForAll> ExprIterAssignedTrigger;
    struct ContainerTrigger;
    struct ContainerIterAssignedTrigger;

    struct DelayedUnrollTrigger;


    std::vector<Value> queueOfValuesToAdd;
    std::vector<std::shared_ptr<ExprTrigger>> exprTriggers;
    std::shared_ptr<ContainerTrigger> containerTrigger = nullptr;
    std::shared_ptr<ContainerIterAssignedTrigger> containerIterAssignedTrigger =
        nullptr;
    std::shared_ptr<DelayedUnrollTrigger> delayedUnrollTrigger = nullptr;
    std::vector<Value> valuesToUnroll;
    OpSetForAll(const OpSetForAll&) = delete;
    OpSetForAll(OpSetForAll&& other);

    OpSetForAll(
        Quantifier<SetReturning, SetValue, BoolReturning, BoolValue> quant,
        const FastIterableIntSet& violatingOperands)
        : Quantifier(std::move(quant)), violatingOperands(violatingOperands) {}
};
#endif /* SRC_OPERATORS_OPSETFORALL_H_ */
