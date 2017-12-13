
#ifndef SRC_OPERATORS_OPSETFORALL_H_
#define SRC_OPERATORS_OPSETFORALL_H_
#include "operators/opAnd.h"
#include "operators/operatorBase.h"
#include "operators/quantifier.h"
#include "types/bool.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"
struct OpSetForAll;
struct OpSetForAll : public BoolView,
                     public BoolQuantifier<OpSetForAll, SetReturning, SetValue,
                                           ConjunctionTrigger<OpSetForAll>> {
    using BoolQuantifier<OpSetForAll, SetReturning, SetValue,
                         ConjunctionTrigger<OpSetForAll>>::BoolQuantifier;
    typedef ConjunctionIterAssignedTrigger<OpSetForAll> ExprIterAssignedTrigger;
    struct ContainerTrigger;
    struct DelayedUnrollTrigger;

    std::shared_ptr<ContainerTrigger> containerTrigger = nullptr;
    std::shared_ptr<DelayedUnrollTrigger> delayedUnrollTrigger = nullptr;
    std::vector<AnyValRef> valuesToUnroll;
    OpSetForAll(SetReturning set) : BoolQuantifier(std::move(set)) {}
    OpSetForAll(const OpSetForAll&) = delete;
    OpSetForAll(OpSetForAll&& other);
    ~OpSetForAll() { stopTriggering(*this); }

    inline void operandRemoved(u_int64_t removedViolation) {
        if (removedViolation > 0) {
            visitTriggers(
                [&](auto& trigger) { trigger->possibleValueChange(violation); },
                triggers);
            violation -= removedViolation;
            visitTriggers(
                [&](auto& trigger) { trigger->valueChanged(violation); },
                triggers);
        }
    }
    inline void operandAdded(u_int64_t addedViolation) {
        if (addedViolation > 0) {
            visitTriggers(
                [&](auto& trigger) { trigger->possibleValueChange(violation); },
                triggers);
            violation += addedViolation;
            visitTriggers(
                [&](auto& trigger) { trigger->valueChanged(violation); },
                triggers);
        }
    }
};
#endif /* SRC_OPERATORS_OPSETFORALL_H_ */
