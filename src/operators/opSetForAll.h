
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
      public BoolQuantifier<OpSetForAll, SetReturning, SetValue,
                            ConjunctionIterAssignedTrigger<OpSetForAll>> {
    using BoolQuantifier<
        OpSetForAll, SetReturning, SetValue,
        ConjunctionIterAssignedTrigger<OpSetForAll>>::BoolQuantifier;
    typedef ConjunctionIterAssignedTrigger<OpSetForAll> ExprIterAssignedTrigger;
    struct ContainerTrigger;
    struct DelayedUnrollTrigger;

    std::shared_ptr<ContainerTrigger> containerTrigger = nullptr;
    std::shared_ptr<DelayedUnrollTrigger> delayedUnrollTrigger = nullptr;
    std::vector<AnyValRef> valuesToUnroll;
    OpSetForAll(const OpSetForAll&) = delete;
    OpSetForAll(OpSetForAll&& other);
    ~OpSetForAll() { stopTriggering(*this); }
};
#endif /* SRC_OPERATORS_OPSETFORALL_H_ */
