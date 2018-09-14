#include "operators/opCatchUndef.h"
#include <iostream>
#include <memory>
#include "triggers/allTriggers.h"
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::addTriggerImpl(
    const shared_ptr<ExprTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    triggers.emplace_back(getTriggerBase(trigger));
    trigger->acceptDefinednessTriggers = false;
    getMember()->addTrigger(trigger, includeMembers, memberIndex);
}

template <typename ExprViewType>
ExprRef<ExprViewType>& OpCatchUndef<ExprViewType>::getMember() {
    return (exprDefined) ? expr : replacement;
}

template <typename ExprViewType>
const ExprRef<ExprViewType>& OpCatchUndef<ExprViewType>::getMember() const {
    return (exprDefined) ? expr : replacement;
}

template <typename ExprViewType>
OptionalRef<ExprViewType> OpCatchUndef<ExprViewType>::view() {
    return getMember()->view();
}
template <typename ExprViewType>
OptionalRef<const ExprViewType> OpCatchUndef<ExprViewType>::view() const {
    return getMember()->view();
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::reevaluate() {
    exprDefined = expr->getViewIfDefined().hasValue();
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::evaluateImpl() {
    replacement->evaluate();
    expr->evaluate();
    reevaluate();
}

template <typename ExprViewType>
OpCatchUndef<ExprViewType>::OpCatchUndef(OpCatchUndef<ExprViewType>&& other)
    : ExprInterface<ExprViewType>(move(other)),
      triggers(move(other.triggers)),
      expr(move(other.expr)),
      replacement(move(other.replacement)),
      exprDefined(move(other.exprDefined)),
      exprTrigger(move(other.exprTrigger)) {
    setTriggerParent(this, exprTrigger);
}
namespace {
template <typename ExprViewType, typename ExprTriggerType, typename Derived>
struct ExprTriggerHelper
    : public OpCatchUndef<ExprViewType>::ExprTriggerBase,
      public ChangeTriggerAdapter<ExprTriggerType, Derived> {
    using OpCatchUndef<ExprViewType>::ExprTriggerBase::ExprTriggerBase;
};

template <typename ExprViewType, typename Derived>
struct ExprTriggerHelper<ExprViewType, TupleTrigger, Derived>
    : public OpCatchUndef<ExprViewType>::ExprTriggerBase,
      public ChangeTriggerAdapter<TupleTrigger, Derived> {
    using OpCatchUndef<ExprViewType>::ExprTriggerBase::ExprTriggerBase;
    void memberHasBecomeDefined(UInt) final {
        auto view = this->op->expr->view();
        if (view && view.get().numberUndefined == 0) {
            static_cast<Derived&>(*this).adapterHasBecomeDefined();
        }
    }
    void memberHasBecomeUndefined(UInt) final {
        auto view = this->op->expr->view();
        if (!view || view.get().numberUndefined == 1) {
            static_cast<Derived&>(*this).adapterHasBecomeUndefined();
        }
    }
};

template <typename ExprViewType, typename TriggerType>
struct ExprTrigger
    : public ExprTriggerHelper<ExprViewType, TriggerType,
                               ExprTrigger<ExprViewType, TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type ExprType;
    using ExprTriggerHelper<
        ExprViewType, TriggerType,
        ExprTrigger<ExprViewType, TriggerType>>::ExprTriggerHelper;

    void adapterValueChanged() {}

    void adapterHasBecomeDefined() {
        this->op->exprDefined = true;
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            this->op->triggers);
    }

    void adapterHasBecomeUndefined() {
        this->op->exprDefined = false;
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            this->op->triggers);
    }
    void reattachTrigger() final {
        deleteTrigger(
            static_pointer_cast<ExprTrigger<ExprViewType, TriggerType>>(
                this->op->exprTrigger));
        auto trigger =
            make_shared<ExprTrigger<ExprViewType, TriggerType>>(this->op);
        this->op->expr->addTrigger(trigger);
        this->op->exprTrigger = trigger;
    }
};
}  // namespace

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::startTriggeringImpl() {
    if (!exprTrigger) {
        typedef typename AssociatedTriggerType<ExprViewType>::type TriggerType;
        auto trigger =
            make_shared<ExprTrigger<ExprViewType, TriggerType>>(this);
        exprTrigger = trigger;
        expr->addTrigger(trigger);
        expr->startTriggering();
    }
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::stopTriggeringOnChildren() {
    if (exprTrigger) {
        deleteTrigger(
            static_pointer_cast<ExprTrigger<ExprViewType, ExprTriggerType>>(
                exprTrigger));
        exprTrigger = nullptr;
    }
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::stopTriggering() {
    if (exprTrigger) {
        stopTriggeringOnChildren();
        expr->stopTriggering();
    }
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    if (exprDefined) {
        expr->updateVarViolations(vioContext, vioContainer);
    }
}

template <typename ExprViewType>
ExprRef<ExprViewType> OpCatchUndef<ExprViewType>::deepCopySelfForUnrollImpl(
    const ExprRef<ExprViewType>&, const AnyIterRef& iterator) const {
    auto newOpCatchUndef = make_shared<OpCatchUndef<ExprViewType>>(
        expr->deepCopySelfForUnroll(expr, iterator), replacement);
    newOpCatchUndef->exprDefined = exprDefined;
    return newOpCatchUndef;
}

template <typename ExprViewType>
std::ostream& OpCatchUndef<ExprViewType>::dumpState(std::ostream& os) const {
    os << "opCatchUndef(exprDefined=" << exprDefined << ",\nexpr=";
    expr->dumpState(os) << ",\n";
    os << "replacement=";
    replacement->dumpState(os) << ")";
    return os;
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->expr = findAndReplace(expr, func);
    this->replacement = findAndReplace(replacement, func);
}

template <typename ExprViewType>
bool OpCatchUndef<ExprViewType>::isUndefined() {
    return false;
}

template <typename ExprViewType>
pair<bool, ExprRef<ExprViewType>> OpCatchUndef<ExprViewType>::optimise(
    PathExtension path) {
    bool changeMade = false;
    auto optResult = expr->optimise(path.extend(expr));
    changeMade |= optResult.first;
    expr = optResult.second;
    optResult = replacement->optimise(path.extend(replacement));
    changeMade |= optResult.first;
    replacement = optResult.second;
    return make_pair(changeMade, mpark::get<ExprRef<ExprViewType>>(path.expr));
}

template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpCatchUndef<View>> {
    static ExprRef<View> make(ExprRef<View> expr, ExprRef<View> replacement);
};

template <typename View>
ExprRef<View> OpMaker<OpCatchUndef<View>>::make(ExprRef<View> expr,
                                                ExprRef<View> replacement) {
    return make_shared<OpCatchUndef<View>>(move(expr), move(replacement));
}

#define opCatchUndefInstantiators(name)       \
    template struct OpCatchUndef<name##View>; \
    template struct OpMaker<OpCatchUndef<name##View>>;

buildForAllTypes(opCatchUndefInstantiators, );
#undef opCatchUndefInstantiators
