#ifndef SRC_OPERATORS_QUANTIFIER_HPP_
#define SRC_OPERATORS_QUANTIFIER_HPP_
#include "operators/quantifier.h"

#include <vector>
#include "triggers/allTriggers.h"

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
Quantifier<ContainerType>::Quantifier(Quantifier<ContainerType>&& other)
    : SequenceView(std::move(other)),
      quantId(other.quantId),
      container(std::move(other.container)),
      expr(std::move(other.expr)),
      unrolledIterVals(std::move(other.unrolledIterVals)),
      containerTrigger(std::move(other.containerTrigger)),
      containerDelayedTrigger(std::move(other.containerDelayedTrigger)),
      exprTriggers(std::move(other.exprTriggers)) {
    setTriggerParent(this, containerTrigger, containerDelayedTrigger,
                     exprTriggers);
}

template <typename ContainerType>
bool Quantifier<ContainerType>::triggering() {
    return static_cast<bool>(containerTrigger);
}

template <typename ContainerType>
template <typename ViewType>
void Quantifier<ContainerType>::unroll(UInt index,
                                       const ExprRef<ViewType>& newView) {
    mpark::visit(
        [&](auto& members) {
            debug_log("unrolling " << newView << " at index " << index);
            auto iterRef = this->newIterRef<ViewType>();
            // choose which expr to copy from
            // use previously unrolled expr if available as  can be most
            // efficiently updated to new unrolled value  otherwise use expr
            // template
            auto exprToCopy = (members.empty())
                                  ? mpark::get<ExprRef<viewType(members)>>(expr)
                                  : members.back();
            // decide if expr that is going to be unrolled needs evaluating.
            // It needs evaluating if we are not in triggering  mode or if
            // the expr being unrolled is taken from the expr template, because
            // the expr template will not have been evaluated before.  If we are
            // instead copying from an already unrolled expr, it  need not be
            // evaluated, simply copy it and trigger the change in value.
            bool evaluateExpr = members.empty();
            const ExprRef<ViewType>& oldValueOfIter =
                (members.empty())
                    ? IterRef<ViewType>(nullptr)
                    : mpark::get<IterRef<ViewType>>(unrolledIterVals.back())
                          ->getValue();
            ExprRef<viewType(members)> newMember =
                exprToCopy->deepCopyForUnroll(exprToCopy, iterRef);
            iterRef->changeValue(!evaluateExpr, oldValueOfIter, newView, [&]() {
                if (evaluateExpr) {
                    newMember->evaluate();
                }
                newMember->startTriggering();
            });
            unrolledIterVals.insert(unrolledIterVals.begin() + index, iterRef);
//            newMember->debugSanityCheck();
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
void Quantifier<ContainerType>::swapExprs(UInt index1, UInt index2) {
    mpark::visit(
        [&](auto& members) {
            this->template swapAndNotify<viewType(members)>(index1, index2);
        },
        members);
    std::swap(exprTriggers[index1], exprTriggers[index2]);
    exprTriggers[index1]->index = index1;
    exprTriggers[index2]->index = index2;
    std::swap(unrolledIterVals[index1], unrolledIterVals[index2]);
}

template <typename ContainerType>
void Quantifier<ContainerType>::roll(UInt index) {
    debug_log("Rolling  index " << index << " with value "
                                << unrolledIterVals[index]);
    mpark::visit(
        [&](auto& members) {
            if (containerDefined) {
                this->template removeMemberAndNotify<viewType(members)>(index);
            } else {
                this->template removeMember<viewType(members)>(index);
            }
            if (this->triggering()) {
                this->stopTriggeringOnExpr<viewType(members)>(index);
            }
        },
        members);
    unrolledIterVals.erase(unrolledIterVals.begin() + index);
}

template <typename ContainerType>
ExprRef<SequenceView> Quantifier<ContainerType>::deepCopyForUnrollImpl(
    const ExprRef<SequenceView>&, const AnyIterRef& iterator) const {
    auto newQuantifier = std::make_shared<Quantifier<ContainerType>>(
        container->deepCopyForUnroll(container, iterator), quantId);

    mpark::visit(
        [&](const auto& vToUnroll) {
            newQuantifier->valuesToUnroll
                .template emplace<ExprRefVec<viewType(vToUnroll)>>();
        },
        this->valuesToUnroll);

    mpark::visit(
        [&](const auto& expr) {
            newQuantifier->setExpression(
                expr->deepCopyForUnroll(expr, iterator));
            auto& members = this->template getMembers<viewType(expr)>();
            for (size_t i = 0; i < members.size(); ++i) {
                auto& member = members[i];
                auto& iterVal = unrolledIterVals[i];
                newQuantifier->template addMember<viewType(members)>(
                    newQuantifier->numberElements(),
                    member->deepCopyForUnroll(member, iterator));
                newQuantifier->unrolledIterVals.emplace_back(iterVal);
            }
        },
        this->expr);
    newQuantifier->containerDefined = containerDefined;
    return newQuantifier;
}

template <typename ContainerType, typename ViewType>
struct ExprChangeTrigger : public ChangeTriggerAdapter<
                               typename AssociatedTriggerType<ViewType>::type,
                               ExprChangeTrigger<ContainerType, ViewType>>,
                           public Quantifier<ContainerType>::ExprTriggerBase {
    typedef typename AssociatedTriggerType<ViewType>::type TriggerType;

    ExprChangeTrigger(Quantifier<ContainerType>* op, UInt index)
        : ChangeTriggerAdapter<TriggerType,
                               ExprChangeTrigger<ContainerType, ViewType>>(
              op->template getMembers<ViewType>()[index]),
          Quantifier<ContainerType>::ExprTriggerBase(op, index) {}

    ExprRef<ViewType>& getTriggeringOperand() {
        return this->op->template getMembers<ViewType>()[this->index];
    }
    void adapterValueChanged() {
        mpark::visit(
            [&](auto& members) {
                this->op
                    ->template changeSubsequenceAndNotify<viewType(members)>(
                        this->index, this->index + 1);
            },
            this->op->members);
    }
    void reattachTrigger() final {
        auto& triggerToChange = this->op->exprTriggers.at(this->index);
        deleteTrigger(
            std::static_pointer_cast<
                ExprChangeTrigger<ContainerType, ViewType>>(triggerToChange));
        auto trigger =
            std::make_shared<ExprChangeTrigger<ContainerType, ViewType>>(
                this->op, this->index);
        this->op->template getMembers<ViewType>()[this->index]->addTrigger(
            trigger);
        triggerToChange = trigger;
    }

    void adapterHasBecomeDefined() {
        mpark::visit(
            [&](auto& members) {
                this->op->template defineMemberAndNotify<viewType(members)>(
                    this->index);
            },
            this->op->members);
    }
    void adapterHasBecomeUndefined() {
        mpark::visit(
            [&](auto& members) {
                this->op->template undefineMemberAndNotify<viewType(members)>(
                    this->index);
            },
            this->op->members);
    }
};

template <typename ContainerType>
template <typename ViewType>
void Quantifier<ContainerType>::startTriggeringOnExpr(UInt index,
                                                      ExprRef<ViewType>& expr) {
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
template <typename ViewType>
void Quantifier<ContainerType>::stopTriggeringOnExpr(UInt oldIndex) {
    deleteTrigger(
        std::static_pointer_cast<ExprChangeTrigger<ContainerType, ViewType>>(
            exprTriggers[oldIndex]));
    exprTriggers.erase(exprTriggers.begin() + oldIndex);
    for (size_t i = oldIndex; i < exprTriggers.size(); i++) {
        exprTriggers[i]->index = i;
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
        mpark::visit(
            [&](auto& expr) {
                while (!exprTriggers.empty()) {
                    this->stopTriggeringOnExpr<viewType(expr)>(
                        exprTriggers.size() - 1);
                }
            },
            expr);
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
    debug_code(assert(this->numberElements() == 0));
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
}

#endif /* SRC_OPERATORS_QUANTIFIER_HPP_ */
