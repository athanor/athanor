#include "operators/opFunctionImage.h"
#include <iostream>
#include <memory>
#include "triggers/allTriggers.h"
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;
#define invoke(expr, func) \
    mpark::visit([&](auto& expr) { return expr->func; }, expr)

#define invoke_r(expr, func, ret) \
    mpark::visit([&](auto& expr) -> ret { return expr->func; }, expr)

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::addTriggerImpl(
    const shared_ptr<FunctionMemberTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    triggers.emplace_back(getTriggerBase(trigger));
    if (locallyDefined) {
        getMember().get()->addTrigger(trigger, includeMembers, memberIndex);
    }
}

template <typename FunctionMemberViewType>
OptionalRef<ExprRef<FunctionMemberViewType>>
OpFunctionImage<FunctionMemberViewType>::getMember() {
    debug_code(assert(this->appearsDefined()));
    debug_code(assert(invoke(preImageOperand, appearsDefined())));
    debug_code(assert(cachedIndex >= 0));
    auto view = functionOperand->getViewIfDefined();
    if (!view || cachedIndex >= (Int)(*view).rangeSize()) {
        return EmptyOptional();
    }

    auto& member = (*view).getRange<FunctionMemberViewType>()[cachedIndex];
    return member;
}

template <typename FunctionMemberViewType>
OptionalRef<const ExprRef<FunctionMemberViewType>>
OpFunctionImage<FunctionMemberViewType>::getMember() const {
    debug_code(assert(this->appearsDefined()));
    debug_code(assert(invoke(preImageOperand, appearsDefined())));
    debug_code(assert(cachedIndex >= 0));
    auto view = functionOperand->getViewIfDefined();
    if (!view || cachedIndex >= (Int)(*view).rangeSize()) {
        return EmptyOptional();
    }
    const auto& member =
        (*view).getRange<FunctionMemberViewType>()[cachedIndex];
    return member;
}

template <typename FunctionMemberViewType>
OptionalRef<FunctionMemberViewType>
OpFunctionImage<FunctionMemberViewType>::view() {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return EmptyOptional();
    }
}
template <typename FunctionMemberViewType>
OptionalRef<const FunctionMemberViewType>
OpFunctionImage<FunctionMemberViewType>::view() const {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return EmptyOptional();
    }
}

template <typename FunctionMemberViewType, typename TriggerType>
struct PreImageTrigger;

template <typename FunctionMemberViewType>
lib::optional<UInt> OpFunctionImage<FunctionMemberViewType>::calculateIndex() {
    auto functionView = functionOperand->view();
    if (!functionView) {
        return lib::nullopt;
    }
    return mpark::visit(
        [&](auto& preImageOperand) -> lib::optional<UInt> {
            auto view = preImageOperand->view();
            if (!view) {
                return lib::nullopt;
            }
            return (*functionView).domainToIndex(*view);
        },
        preImageOperand);
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::reevaluate(
    bool recalculateCachedIndex) {
    if (!invoke(preImageOperand, appearsDefined())) {
        locallyDefined = false;
        this->setAppearsDefined(false);
        return;
    }
    if (recalculateCachedIndex) {
        auto index = calculateIndex();
        if (!index) {
            locallyDefined = false;
        } else {
            locallyDefined = true;
            cachedIndex = *index;
        }
    }
    setAppearsDefined(locallyDefined && getMember().get()->appearsDefined());
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::evaluateImpl() {
    functionOperand->evaluate();
    invoke(preImageOperand, evaluate());
    reevaluate();
}

template <typename FunctionMemberViewType>
OpFunctionImage<FunctionMemberViewType>::OpFunctionImage(
    OpFunctionImage<FunctionMemberViewType>&& other)
    : ExprInterface<FunctionMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      functionOperand(move(other.functionOperand)),
      preImageOperand(move(other.preImageOperand)),
      cachedIndex(move(other.cachedIndex)),
      locallyDefined(move(other.locallyDefined)),
      functionOperandTrigger(move(other.functionOperandTrigger)),
      functionMemberTrigger(move(other.functionMemberTrigger)),
      preImageTrigger(move(other.preImageTrigger)) {
    setTriggerParent(this, functionOperandTrigger, functionMemberTrigger,
                     preImageTrigger);
}

template <typename FunctionMemberViewType>
bool OpFunctionImage<FunctionMemberViewType>::eventForwardedAsDefinednessChange(
    bool recalculateIndex) {
    bool wasDefined = this->appearsDefined();
    bool wasLocallyDefined = locallyDefined;
    reevaluate(recalculateIndex);
    if (!wasLocallyDefined && locallyDefined) {
        reattachFunctionMemberTrigger(true);
    }
    if (wasDefined && !this->appearsDefined()) {
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, triggers,
                      true);
        return true;
    } else if (!wasDefined && this->appearsDefined()) {
        visitTriggers(
            [&](auto t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            triggers, true);
        return true;
    } else {
        return !this->appearsDefined();
    }
}

template <typename FunctionMemberViewType>
struct OpFunctionImage<FunctionMemberViewType>::FunctionOperandTrigger
    : public FunctionTrigger {
    OpFunctionImage<FunctionMemberViewType>* op;
    FunctionOperandTrigger(OpFunctionImage<FunctionMemberViewType>* op)
        : op(op) {}

    void imageChanged(UInt) {
        // ignore, already triggering on member
    }

    void imageChanged(const std::vector<UInt>&) {
        // ignore, already triggering
    }

    void imageSwap(UInt index1, UInt index2) final {
        if (!op->locallyDefined) {
            return;
        }
        debug_code(assert((Int)index1 == op->cachedIndex ||
                          (Int)index2 == op->cachedIndex));
        if (index1 == index2) {
            return;
        }
        if ((Int)index1 != op->cachedIndex) {
            swap(index1, index2);
        }
        if (!op->eventForwardedAsDefinednessChange(false)) {
            visitTriggers(
                [&](auto t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
            op->reattachFunctionMemberTrigger(true);
        }
    }

    void valueChanged() final {
        if (!op->eventForwardedAsDefinednessChange(true)) {
            visitTriggers(
                [&](auto& t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
        }
    }

    inline void hasBecomeUndefined() {
        op->locallyDefined = false;
        if (!op->appearsDefined()) {
            return;
        }
        op->setAppearsDefined(false);
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers,
                      true);
    }

    void hasBecomeDefined() {
        op->reevaluate(true);
        if (!op->appearsDefined()) {
            return;
        }
        op->reattachFunctionMemberTrigger(true);
        visitTriggers(
            [&](auto t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            op->triggers, true);
    }
    void reattachTrigger() final {
        deleteTrigger(op->functionOperandTrigger);
        auto trigger = make_shared<FunctionOperandTrigger>(op);
        op->functionOperand->addTrigger(trigger, false);
        if (op->locallyDefined) {
            op->reattachFunctionMemberTrigger(true);
        }
        op->functionOperandTrigger = trigger;
    }
};

template <typename FunctionMemberViewType, typename TriggerType>
struct PreImageTrigger
    : public ChangeTriggerAdapter<
          TriggerType, PreImageTrigger<FunctionMemberViewType, TriggerType>>,
      public OpFunctionImage<FunctionMemberViewType>::PreImageTriggerBase {
    typedef typename AssociatedViewType<TriggerType>::type PreImageType;
    using OpFunctionImage<
        FunctionMemberViewType>::PreImageTriggerBase::PreImageTriggerBase;
    using OpFunctionImage<FunctionMemberViewType>::PreImageTriggerBase::op;

    ExprRef<PreImageType>& getTriggeringOperand() {
        return mpark::get<ExprRef<PreImageType>>(op->preImageOperand);
    }
    void adapterValueChanged() {
        if (!op->eventForwardedAsDefinednessChange(true)) {
            visitTriggers(
                [&](auto t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
            op->reattachFunctionMemberTrigger(true);
        }
    }
    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<
                      PreImageTrigger<FunctionMemberViewType, TriggerType>>(
            op->preImageTrigger));
        auto trigger =
            make_shared<PreImageTrigger<FunctionMemberViewType, TriggerType>>(
                op);
        mpark::get<ExprRef<PreImageType>>(op->preImageOperand)
            ->addTrigger(trigger);
        op->preImageTrigger = trigger;
    }
    void adapterHasBecomeDefined() {
        op->reevaluate();
        if (op->appearsDefined()) {
            return;
        }
        visitTriggers(
            [&](auto t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            op->triggers, true);
    }

    void adapterHasBecomeUndefined() {
        if (!op->appearsDefined()) {
            return;
        }
        op->locallyDefined = false;
        op->setAppearsDefined(false);
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers,
                      true);
    }
};

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::startTriggeringImpl() {
    if (!preImageTrigger) {
        mpark::visit(
            [&](auto& preImageOperand) {
                typedef typename AssociatedTriggerType<viewType(
                    preImageOperand)>::type TriggerType;
                auto trigger = make_shared<
                    PreImageTrigger<FunctionMemberViewType, TriggerType>>(this);
                preImageTrigger = trigger;
                preImageOperand->addTrigger(trigger);
                preImageOperand->startTriggering();
            },
            preImageOperand);
        functionOperandTrigger = make_shared<
            OpFunctionImage<FunctionMemberViewType>::FunctionOperandTrigger>(
            this);
        functionOperand->addTrigger(functionOperandTrigger, false);
        reattachFunctionMemberTrigger(false);
        functionOperand->startTriggering();
    }
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::reattachFunctionMemberTrigger(
    bool deleteFirst) {
    if (deleteFirst) {
        deleteTrigger(functionMemberTrigger);
    }
    functionMemberTrigger = make_shared<FunctionOperandTrigger>(this);
    functionOperand->addTrigger(functionMemberTrigger, true, cachedIndex);
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::stopTriggeringOnChildren() {
    if (preImageTrigger) {
        mpark::visit(
            [&](auto& preImageOperand) {
                typedef typename AssociatedTriggerType<viewType(
                    preImageOperand)>::type PreImageTriggerType;
                deleteTrigger(
                    static_pointer_cast<PreImageTrigger<FunctionMemberViewType,
                                                        PreImageTriggerType>>(
                        preImageTrigger));
            },
            preImageOperand);
        deleteTrigger(functionOperandTrigger);
        deleteTrigger(functionMemberTrigger);
        preImageTrigger = nullptr;
        functionOperandTrigger = nullptr;
        functionMemberTrigger = nullptr;
    }
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::stopTriggering() {
    if (preImageTrigger) {
        stopTriggeringOnChildren();
        invoke(preImageOperand, stopTriggering());
        ;
        functionOperand->stopTriggering();
    }
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    invoke(preImageOperand, updateVarViolationsImpl(vioContext, vioContainer));
    auto functionMember = getMember();
    if (locallyDefined && functionMember) {
        (*functionMember)->updateVarViolations(vioContext, vioContainer);
    } else {
        functionOperand->updateVarViolations(vioContext, vioContainer);
    }
}

template <typename FunctionMemberViewType>
ExprRef<FunctionMemberViewType>
OpFunctionImage<FunctionMemberViewType>::deepCopySelfForUnrollImpl(
    const ExprRef<FunctionMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpFunctionImage =
        make_shared<OpFunctionImage<FunctionMemberViewType>>(
            functionOperand->deepCopySelfForUnroll(functionOperand, iterator),
            invoke_r(preImageOperand,
                     deepCopySelfForUnrollImpl(preImageOperand, iterator),
                     AnyExprRef));
    newOpFunctionImage->cachedIndex = cachedIndex;
    newOpFunctionImage->locallyDefined = locallyDefined;
    return newOpFunctionImage;
}

template <typename FunctionMemberViewType>
std::ostream& OpFunctionImage<FunctionMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opFunctionImage(value=";
    prettyPrint(os, this->getViewIfDefined());
    os << ",\n";

    os << "function=";
    functionOperand->dumpState(os) << ",\n";
    os << "preImage=";
    invoke_r(preImageOperand, dumpState(os), ostream&) << ")";
    return os;
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->functionOperand = findAndReplace(functionOperand, func);
    this->preImageOperand = mpark::visit(
        [&](auto& preImageOperand) -> AnyExprRef {
            return findAndReplace(preImageOperand, func);
        },
        preImageOperand);
}
template <typename FunctionMemberViewType>
pair<bool, ExprRef<FunctionMemberViewType>>
OpFunctionImage<FunctionMemberViewType>::optimise(PathExtension path) {
    bool changeMade = false;
    auto optResult = functionOperand->optimise(path.extend(functionOperand));
    changeMade |= optResult.first;
    functionOperand = optResult.second;
    mpark::visit(
        [&](auto& preImageOperand) {
            auto optResult =
                preImageOperand->optimise(path.extend(preImageOperand));
            changeMade |= optResult.first;
            preImageOperand = optResult.second;
        },
        preImageOperand);
    return make_pair(changeMade,
                     mpark::get<ExprRef<FunctionMemberViewType>>(path.expr));
}
template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpFunctionImage<View>> {
    static ExprRef<View> make(ExprRef<FunctionView> function,
                              AnyExprRef preImage);
};

template <typename View>
ExprRef<View> OpMaker<OpFunctionImage<View>>::make(
    ExprRef<FunctionView> function, AnyExprRef preImage) {
    return make_shared<OpFunctionImage<View>>(move(function), move(preImage));
}

#define opFunctionImageInstantiators(name)       \
    template struct OpFunctionImage<name##View>; \
    template struct OpMaker<OpFunctionImage<name##View>>;

buildForAllTypes(opFunctionImageInstantiators, );
#undef opFunctionImageInstantiators
