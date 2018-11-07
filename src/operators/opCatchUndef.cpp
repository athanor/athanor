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
    handleTriggerAdd(trigger, includeMembers, memberIndex, *this);
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
void OpCatchUndef<ExprViewType>::setAppearsDefined(bool set) {
    // assign exprDefined but remember old value
    swap(set, exprDefined);
    if (set != exprDefined) {
        recentlyTriggeredChange = true;
        visitTriggers([&](auto& t) { t->valueChanged(); }, this->triggers);
    }
}
template <typename ExprViewType>
bool OpCatchUndef<ExprViewType>::allowForwardingOfTrigger() {
    if (recentlyTriggeredChange) {
        recentlyTriggeredChange = false;
        return false;
    }
    return exprDefined;
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::reevaluate() {
    exprDefined = expr->getViewIfDefined().hasValue();
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::evaluateImpl() {
    replacement->evaluate();
    expr->evaluate();
    // if not bool view invoke set appears defined true
    // careful to invoke setAppearsDefined on the base class
    // ExprInterface::setAppearsDefined, not OpCatchUndef::setAppearsDefined
    overloaded([&](ExprInterface<BoolView>&) {},
               [&](auto& expr) { expr.setAppearsDefined(true); })(
        static_cast<ExprInterface<ExprViewType>&>(*this));
    reevaluate();
}

template <typename ExprViewType>
OpCatchUndef<ExprViewType>::OpCatchUndef(OpCatchUndef<ExprViewType>&& other)
    : ExprInterface<ExprViewType>(move(other)),
      TriggerContainer<ExprViewType>(move(other)),
      expr(move(other.expr)),
      replacement(move(other.replacement)),
      exprDefined(move(other.exprDefined)),
      exprTrigger(move(other.exprTrigger)) {
    setTriggerParent(this, exprTrigger);
}

template <typename ExprViewType>
struct OpCatchUndef<ExprViewType>::ExprTrigger
    : public ForwardingTrigger<
          typename AssociatedTriggerType<ExprViewType>::type,
          OpCatchUndef<ExprViewType>,
          typename OpCatchUndef<ExprViewType>::ExprTrigger> {
    using ForwardingTrigger<
        typename AssociatedTriggerType<ExprViewType>::type,
        OpCatchUndef<ExprViewType>,
        typename OpCatchUndef<ExprViewType>::ExprTrigger>::ForwardingTrigger;
    ExprRef<ExprViewType>& getTriggeringOperand() { return this->op->expr; }

    void reattachTrigger() final {
        deleteTrigger(this->op->exprTrigger);
        auto trigger =
            make_shared<OpCatchUndef<ExprViewType>::ExprTrigger>(this->op);
        this->op->expr->addTrigger(trigger);
        this->op->exprTrigger = trigger;
    }
};

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::startTriggeringImpl() {
    if (!exprTrigger) {
        exprTrigger =
            make_shared<typename OpCatchUndef<ExprViewType>::ExprTrigger>(this);
        expr->addTrigger(exprTrigger);
        expr->startTriggering();
    }
}

template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::stopTriggeringOnChildren() {
    if (exprTrigger) {
        deleteTrigger(exprTrigger);
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
ExprRef<ExprViewType> OpCatchUndef<ExprViewType>::deepCopyForUnrollImpl(
    const ExprRef<ExprViewType>&, const AnyIterRef& iterator) const {
    auto newOpCatchUndef = make_shared<OpCatchUndef<ExprViewType>>(
        expr->deepCopyForUnroll(expr, iterator), replacement);
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

template <typename ExprViewType>
string OpCatchUndef<ExprViewType>::getOpName() const {
    return "OpCatchUndef";
}
template <typename ExprViewType>
void OpCatchUndef<ExprViewType>::debugSanityCheckImpl() const {
    expr->debugSanityCheck();
    replacement->debugSanityCheck();
    sanityCheck(this->appearsDefined(), "This op should not be undefined.");
    sanityCheck(exprDefined == expr->appearsDefined(),
                toString("exprDefined should be ", expr->appearsDefined(),
                         " but it is actually ", exprDefined));
    sanityEqualsCheck(true, this->appearsDefined());
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
