#ifndef SRC_OPERATORS_QUANTIFIER_HPP_
#define SRC_OPERATORS_QUANTIFIER_HPP_
#include <vector>
#include "operators/quantifier.h"
#include "operators/quantifierTriggers.h"
#include "triggers/allTriggers.h"
extern bool runSanityChecks;
template <typename ContainerType>
struct InitialUnroller;

template <>
struct InitialUnroller<MSetView>;

template <>
struct InitialUnroller<SetView>;
template <>
struct InitialUnroller<MSetView>;
template <>
struct InitialUnroller<SequenceView>;

template <>
struct InitialUnroller<FunctionView>;

template <typename ContainerType>
struct ContainerSanityChecker;

template <>
struct ContainerSanityChecker<MSetView>;
template <>
struct ContainerSanityChecker<SetView>;
template <>
struct ContainerSanityChecker<SequenceView>;
template <>
struct ContainerSanityChecker<FunctionView>;

template <typename Container>
struct ContainerTrigger;
template <>
struct ContainerTrigger<SetView>;
template <>
struct ContainerTrigger<MSetView>;
template <>
struct ContainerTrigger<SequenceView>;
template <>
struct ContainerTrigger<FunctionView>;

template <typename ContainerType>
bool Quantifier<ContainerType>::triggering() {
    return static_cast<bool>(containerTrigger);
}

template <typename View1, typename View2>
ExprRef<View1> deepCopyExprAndAssignNewValue(ExprRef<View1> exprToCopy,
                                             ExprRef<View2> newValue,
                                             IterRef<View2> iterRef,
                                             bool oldValueNeedsReplacing) {
    ExprRef<View1> newMember =
        exprToCopy->deepCopyForUnroll(exprToCopy, iterRef);
    iterRef->changeValue(oldValueNeedsReplacing, newValue, [&]() {
        if (!oldValueNeedsReplacing) {
            newMember->evaluate();
        }
        newMember->startTriggering();
    });
    if (runSanityChecks) {
        newMember->debugSanityCheck();
    }
    return newMember;
}

template <typename ContainerType>
template <typename View>
typename Quantifier<ContainerType>::UnrolledCondition&
Quantifier<ContainerType>::unrollCondition(UInt index, ExprRef<View> newView,
                                           IterRef<View> iterRef) {
    debug_log("unrolling condition on value " << newView << " at index "
                                              << index);
    // choose which condition to copy from.
    // If no previous unrolled condition, copy from condition template and
    // evaluate from scratch. Otherwise, copy from previously unrolled
    // condition, don't evaluate, just trigger the change in value to the new
    // iterRef.
    auto conditionToCopy = (unrolledConditions.empty())
                               ? condition
                               : unrolledConditions.back().condition;
    bool oldValueNeedsReplacing = !unrolledConditions.empty();
    auto newMember = deepCopyExprAndAssignNewValue(
        conditionToCopy, newView, iterRef, oldValueNeedsReplacing);
    // find where the associated expr should be unrolled.
    UInt exprIndex;
    if (newMember->view().get().violation == 0) {
        // this condition is true, so  the expr should be unrolled after the
        // previous expr with a true condition. if there is no previous one than
        // the previous expr index will have value UNASSIGNED_EXPR_INDEX which
        // is max UInt. + 1 on this will wrap to 0.
        exprIndex =
            (index == 0) ? 0 : unrolledConditions[index - 1].exprIndex + 1;
        std::for_each(unrolledConditions.begin() + index,
                      unrolledConditions.end(),
                      [](auto& cond) { ++cond.exprIndex; });
    } else {
        // condition is false, so match the index of the previous expression so
        // that we keep track of where this expr should be inserted if the
        // condition becomes true.
        exprIndex = (index == 0) ? UnrolledCondition::UNASSIGNED_EXPR_INDEX
                                 : unrolledConditions[index - 1].exprIndex;
    }
    unrolledConditions.insert(unrolledConditions.begin() + index,
                              UnrolledCondition(newMember, exprIndex));
    this->startTriggeringOnCondition(index);
    return unrolledConditions[index];
}

template <typename ContainerType>
template <typename View>
void Quantifier<ContainerType>::unrollExpr(UInt index, ExprRef<View> newView,
                                           IterRef<View> iterRef) {
    mpark::visit(
        [&](auto& members) {
            debug_log("unrolling expr for value " << newView << " at index "
                                                  << index);
            // choose which expr to copy from.
            // If no previous unrolled expr, copy from expr template and
            // evaluate from scratch. Otherwise, copy from previously unrolled
            // expr, don't evaluate, just trigger the change in value to the new
            // iterRef.
            auto exprToCopy = (members.empty())
                                  ? mpark::get<ExprRef<viewType(members)>>(expr)
                                  : members.back();
            bool oldValueNeedsReplacing = !members.empty();
            auto newMember = deepCopyExprAndAssignNewValue(
                exprToCopy, newView, iterRef, oldValueNeedsReplacing);
            if (containerDefined) {
                this->addMemberAndNotify(index, newMember);
            } else {
                this->addMember(index, newMember);
            }
            this->startTriggeringOnExpr(index, newMember);
        },
        members);
}

template <typename ContainerType>
template <typename View>
void Quantifier<ContainerType>::unroll(QueuedUnrollValue<View> queuedValue) {
    auto newIter = this->newIterRef<View>();
    if (!queuedValue.directUnrollExpr) {
        unrolledIterVals.emplace_back(newIter);
    }
    if (!condition || queuedValue.directUnrollExpr) {
        this->unrollExpr(queuedValue.index, queuedValue.value, newIter);
    } else {
        auto& unrolledCondition = this->unrollCondition(
            queuedValue.index, queuedValue.value, newIter);
        if (unrolledCondition.cachedValue) {
            auto tempNewIter = this->newIterRef<View>();
            unrollExpr(unrolledCondition.exprIndex, queuedValue.value,
                       tempNewIter);
            newIter->moveTriggersFrom(*tempNewIter);
        }
    }
}
/**
 the following three functions are helper methods for the
 notifyContainerElementsSwapped
 */
template <typename ContainerType>
inline void normalExprSwap(Quantifier<ContainerType>& quant, UInt index1,
                           UInt index2) {
    mpark::visit(
        [&](auto& members) {
            quant.template swapAndNotify<viewType(members)>(index1, index2);
        },
        quant.members);
    std::swap(quant.exprTriggers[index1], quant.exprTriggers[index2]);
    quant.exprTriggers[index1]->index = index1;
    quant.exprTriggers[index2]->index = index2;
}

/** function handles the case when two unrolled conditions have been swapped,
 * the left one (index1) was true and the right one (index2) was false
 */
template <typename ContainerType>
inline void swappedTrueConditionRight(Quantifier<ContainerType>& quant,
                                      UInt index1, UInt index2) {
    mpark::visit(
        [&](auto& members) {
            typedef viewType(members) View;
            auto removedMember = quant.template removeMemberAndNotify<View>(
                quant.unrolledConditions[index1].exprIndex);
            quant.addMemberAndNotify(quant.unrolledConditions[index2].exprIndex,
                                     removedMember);
        },
        quant.members);
    for (size_t i = quant.unrolledConditions[index1].exprIndex;
         i <= quant.unrolledConditions[index2].exprIndex; i++) {
        quant.exprTriggers[i]->index = i;
    }
    std::for_each(quant.unrolledConditions.begin() + index1,
                  quant.unrolledConditions.begin() + index2,
                  [&](auto&& cond) { --cond.exprIndex; });
}

/** function handles the case when two unrolled conditions have been swapped,
 * the right one (index2) was true and the left one (index1) was false
 */

template <typename ContainerType>
inline void swappedTrueConditionLeft(Quantifier<ContainerType>& quant,
                                     UInt index1, UInt index2) {
    mpark::visit(
        [&](auto& members) {
            typedef viewType(members) View;
            auto removedMember = quant.template removeMemberAndNotify<View>(
                quant.unrolledConditions[index2].exprIndex);
            quant.addMemberAndNotify(
                quant.unrolledConditions[index1].exprIndex + 1, removedMember);
        },
        quant.members);
    for (size_t i = quant.unrolledConditions[index1].exprIndex + 1;
         i <= quant.unrolledConditions[index2].exprIndex; i++) {
        quant.exprTriggers[i]->index = i;
    }
    std::for_each(quant.unrolledConditions.begin() + index1,
                  quant.unrolledConditions.begin() + index2,
                  [&](auto&& cond) { ++cond.exprIndex; });
}

template <typename ContainerType>
void Quantifier<ContainerType>::notifyContainerMembersSwapped(UInt index1,
                                                              UInt index2) {
    if (index1 > index2) {
        std::swap(index1, index2);
    } else if (index1 == index2) {
        return;
    }
    debug_log(index1 << "," << index2);
    std::swap(unrolledIterVals[index1], unrolledIterVals[index2]);
    if (!condition) {
        normalExprSwap(*this, index1, index2);
        return;
    }
    bool leftConditionTrue = unrolledConditions[index1].cachedValue;
    bool rightConditionTrue = unrolledConditions[index2].cachedValue;
    std::swap(unrolledConditions[index1].condition,
              unrolledConditions[index2].condition);
    std::swap(unrolledConditions[index1].trigger,
              unrolledConditions[index2].trigger);
    std::swap(unrolledConditions[index1].cachedValue,
              unrolledConditions[index2].cachedValue);
    unrolledConditions[index1].trigger->index = index1;
    unrolledConditions[index2].trigger->index = index2;
    if (leftConditionTrue && rightConditionTrue) {
        normalExprSwap(*this, unrolledConditions[index1].exprIndex,
                       unrolledConditions[index2].exprIndex);
    } else if (leftConditionTrue && !rightConditionTrue) {
        swappedTrueConditionRight(*this, index1, index2);
    } else if (!leftConditionTrue && rightConditionTrue) {
        swappedTrueConditionLeft(*this, index1, index2);
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::roll(UInt index) {
    debug_code(assert(index < unrolledIterVals.size()));
    debug_log("rolling index " << index << " with value "
                               << unrolledIterVals[index]);
    unrolledIterVals.erase(unrolledIterVals.begin() + index);
    if (!condition) {
        rollExpr(index);
    } else {
        auto condition = rollCondition(index);
        if (condition.cachedValue) {
            rollExpr(condition.exprIndex);
            std::for_each(unrolledConditions.begin() + index,
                          unrolledConditions.end(),
                          [](auto& cond) { --cond.exprIndex; });
        }
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::rollExpr(UInt index) {
    debug_log("Rolling  expr index " << index);
    mpark::visit(
        [&](auto& members) {
            if (containerDefined) {
                this->template removeMemberAndNotify<viewType(members)>(index);
            } else {
                this->template removeMember<viewType(members)>(index);
            }
            if (this->triggering()) {
                this->stopTriggeringOnExpr(index);
            }
        },
        members);
}

template <typename ContainerType>
typename Quantifier<ContainerType>::UnrolledCondition
Quantifier<ContainerType>::rollCondition(UInt index) {
    debug_log("Rolling  condition index " << index);
    debug_code(assert(index < unrolledConditions.size()));
    auto condition = std::move(unrolledConditions[index]);
    unrolledConditions.erase(unrolledConditions.begin() + index);
    stopTriggeringOnCondition(index, condition);
    return condition;
}

template <typename ContainerType>
ExprRef<SequenceView> Quantifier<ContainerType>::deepCopyForUnrollImpl(
    const ExprRef<SequenceView>&, const AnyIterRef& iterator) const {
    auto newQuantifier = std::make_shared<Quantifier<ContainerType>>(
        container->deepCopyForUnroll(container, iterator), quantId);
    newQuantifier->valuesToUnroll = valuesToUnroll;

    mpark::visit(
        [&](const auto& expr) {
            newQuantifier->setExpression(
                expr->deepCopyForUnroll(expr, iterator));
            if (this->isEvaluated()) {
                newQuantifier->evaluateImpl();
            }
        },
        this->expr);
    return newQuantifier;
}

template <typename ContainerType>
template <typename View>
void Quantifier<ContainerType>::startTriggeringOnExpr(UInt index,
                                                      ExprRef<View>& expr) {
    auto trigger =
        std::make_shared<ExprChangeTrigger<ContainerType, viewType(expr)>>(
            this, index);
    exprTriggers.insert(exprTriggers.begin() + index, trigger);
    for (size_t i = index + 1; i < exprTriggers.size(); i++) {
        exprTriggers[i]->index = i;
    }
    expr->addTrigger(trigger);
}

template <typename ContainerType>
void Quantifier<ContainerType>::startTriggeringOnCondition(UInt index) {
    auto trigger =
        std::make_shared<ConditionChangeTrigger<ContainerType>>(this, index);
    unrolledConditions[index].trigger = trigger;
    for (size_t i = index + 1; i < unrolledConditions.size(); i++) {
        unrolledConditions[i].trigger->index = i;
    }
    unrolledConditions[index].condition->addTrigger(trigger);
}

template <typename ContainerType>
void Quantifier<ContainerType>::stopTriggeringOnExpr(UInt oldIndex) {
    deleteTrigger(exprTriggers[oldIndex]);
    exprTriggers.erase(exprTriggers.begin() + oldIndex);
    for (size_t i = oldIndex; i < exprTriggers.size(); i++) {
        exprTriggers[i]->index = i;
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::stopTriggeringOnCondition(
    UInt oldIndex, Quantifier<ContainerType>::UnrolledCondition& condition) {
    deleteTrigger(condition.trigger);
    for (size_t i = oldIndex; i < unrolledConditions.size(); i++) {
        unrolledConditions[i].trigger->index = i;
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::startTriggeringImpl() {
    if (!containerTrigger) {
        containerTrigger =
            std::make_shared<ContainerTrigger<ContainerType>>(this);
        container->addTrigger(containerTrigger);
        container->startTriggering();
    }
    if (!exprTriggers.empty()) {
        return;
    }
    mpark::visit(
        [&](auto& members) {
            for (size_t i = 0; i < members.size(); i++) {
                this->startTriggeringOnExpr<viewType(members)>(i, members[i]);
                members[i]->startTriggering();
            }
        },
        members);
}

template <typename ContainerType>
void Quantifier<ContainerType>::stopTriggeringOnChildren() {
    if (containerTrigger) {
        deleteTrigger(containerTrigger);
        containerTrigger = nullptr;
    }
    if (containerDelayedTrigger) {
        deleteTrigger(containerDelayedTrigger);
        containerDelayedTrigger = nullptr;
    }
    if (!exprTriggers.empty()) {
        while (!exprTriggers.empty()) {
            this->stopTriggeringOnExpr(exprTriggers.size() - 1);
        }
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::stopTriggering() {
    if (containerTrigger) {
        stopTriggeringOnChildren();
        mpark::visit(
            [&](auto& expr) {
                for (auto& member :
                     this->template getMembers<viewType(expr)>()) {
                    member->stopTriggering();
                }
            },
            expr);
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::evaluateImpl() {
    debug_code(assert(unrolledIterVals.empty()));
    debug_code(assert(this->numberUnrolled() == 0));
    container->evaluate();
    auto view = container->getViewIfDefined();
    if (!view) {
        containerDefined = false;
        this->setAppearsDefined(false);
    } else {
        containerDefined = true;
        this->setAppearsDefined(true);
        InitialUnroller<ContainerType>::initialUnroll(*this, *view);
    }
}
template <typename ContainerType>
std::ostream& Quantifier<ContainerType>::dumpState(std::ostream& os) const {
    mpark::visit(
        [&](auto& members) {
            os << "quantifier(container=";
            container->dumpState(os) << ",\n";
            os << "unrolledExprs=[";
            bool first = true;
            for (auto& unrolledExpr : members) {
                if (first) {
                    first = false;
                } else {
                    os << ",\n";
                }
                unrolledExpr->dumpState(os);
            }
            os << "]";
        },
        this->members);
    return os;
}

template <typename ContainerType>
void Quantifier<ContainerType>::updateVarViolationsImpl(const ViolationContext&,
                                                        ViolationContainer&) {}

template <typename ContainerType>
void Quantifier<ContainerType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    mpark::visit([&](auto& expr) { expr = findAndReplace(expr, func); },
                 this->expr);
}

template <typename ContainerType>
std::string Quantifier<ContainerType>::getOpName() const {
    return toString(
        "Quantifier<",
        TypeAsString<typename AssociatedValueType<ContainerType>::type>::value,
        ">");
}

template <typename ContainerType>
void Quantifier<ContainerType>::debugSanityCheckImpl() const {
    container->debugSanityCheck();

    if (!container->appearsDefined()) {
        sanityCheck(!this->appearsDefined(),
                    "Operator should be undefined as container is undefined.");
    }
    auto view = container->view();
    if (!view) {
        return;
    }
    ContainerSanityChecker<viewType(container)>::debugSanityCheck(*this, *view);
    UInt checkNumberUndefined = 0;
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member->debugSanityCheck();
                if (!member->getViewIfDefined().hasValue()) {
                    ++checkNumberUndefined;
                }
            }
            sanityEqualsCheck(checkNumberUndefined, numberUndefined);
        },
        this->members);
    if (checkNumberUndefined == 0) {
        sanityCheck(this->appearsDefined(), "operator should be defined.");
    } else {
        sanityCheck(!this->appearsDefined(), "operator should be undefined.");
    }
    if (condition) {
        UInt numberTrueConditions = 0;
        for (size_t i = 0; i < unrolledConditions.size(); i++) {
            unrolledConditions[i].condition->debugSanityCheck();
            sanityEqualsCheck(
                unrolledConditions[i].condition->view().get().violation == 0,
                unrolledConditions[i].cachedValue);
            if (unrolledConditions[i].cachedValue) {
                ++numberTrueConditions;
            }
            sanityEqualsCheck(numberTrueConditions - 1,
                              unrolledConditions[i].exprIndex);
            // yes indeed if numberTrueConditions is 0 here, it will wrap
            // round to max UInt, which is what we want
        }
    }
}

#endif /* SRC_OPERATORS_QUANTIFIER_HPP_ */
