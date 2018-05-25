#include "operators/opFunctionImage.h"
#include <iostream>
#include <memory>
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;
#define invoke(expr, func) \
    mpark::visit([&](auto& expr) { return expr->func; }, expr)

#define invoke_r(expr, func, ret) \
    mpark::visit([&](auto& expr) -> ret { return expr->func; }, expr)

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::addTrigger(
    const shared_ptr<FunctionMemberTriggerType>& trigger, bool includeMembers) {
    triggers.emplace_back(getTriggerBase(trigger));
    if (locallyDefined) {
        getMember()->addTrigger(trigger, includeMembers);
    }
}

template <typename FunctionMemberViewType>
ExprRef<FunctionMemberViewType>&
OpFunctionImage<FunctionMemberViewType>::getMember() {
    debug_code(assert(locallyDefined));
    debug_code(assert(!invoke(preImageOperand, isUndefined()) &&
                      cachedIndex >= 0 && !functionOperand->isUndefined() &&
                      cachedIndex < (Int)functionOperand->view().rangeSize()));
    auto& member =
        functionOperand->view().getRange<FunctionMemberViewType>()[cachedIndex];
    debug_code(assert(!member->isUndefined()));
    return member;
}

template <typename FunctionMemberViewType>
const ExprRef<FunctionMemberViewType>&
OpFunctionImage<FunctionMemberViewType>::getMember() const {
    debug_code(assert(locallyDefined));
    debug_code(assert(!invoke(preImageOperand, isUndefined()) &&
                      cachedIndex >= 0 && !functionOperand->isUndefined() &&
                      cachedIndex < (Int)functionOperand->view().rangeSize()));

    const auto& member =
        functionOperand->view().getRange<FunctionMemberViewType>()[cachedIndex];
    debug_code(assert(!member->isUndefined()));
    return member;
}

template <typename FunctionMemberViewType>
FunctionMemberViewType& OpFunctionImage<FunctionMemberViewType>::view() {
    return getMember()->view();
}
template <typename FunctionMemberViewType>
const FunctionMemberViewType& OpFunctionImage<FunctionMemberViewType>::view()
    const {
    return getMember()->view();
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::reevaluate() {
    if (invoke(preImageOperand, isUndefined())) {
        locallyDefined = false;
        return;
    }
    auto boolIndexPair = mpark::visit(
        [&](auto& preImageOperand) {
            return functionOperand->view().domainToIndex(
                preImageOperand->view());
        },
        preImageOperand);
    if (!boolIndexPair.first) {
        locallyDefined = false;
    } else {
        locallyDefined = true;
        cachedIndex = boolIndexPair.second;
    }
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
      preImageTrigger(move(other.preImageTrigger)) {
    setTriggerParent(this, preImageTrigger);
}

template <typename FunctionMemberViewType, typename PreImageTriggerType,
          typename Derived>
struct PreImageTriggerHelper
    : public OpFunctionImage<FunctionMemberViewType>::PreImageTriggerBase,
      public ChangeTriggerAdapter<PreImageTriggerType, Derived> {
    using OpFunctionImage<
        FunctionMemberViewType>::PreImageTriggerBase::PreImageTriggerBase;
};

template <typename FunctionMemberViewType, typename Derived>
struct PreImageTriggerHelper<FunctionMemberViewType, TupleTrigger, Derived>
    : public OpFunctionImage<FunctionMemberViewType>::PreImageTriggerBase,
      public ChangeTriggerAdapter<TupleTrigger, Derived> {
    using OpFunctionImage<
        FunctionMemberViewType>::PreImageTriggerBase::PreImageTriggerBase;
    void memberHasBecomeDefined(UInt) final {
        if (mpark::get<ExprRef<TupleView>>(this->op->preImageOperand)
                ->view()
                .numberUndefined == 0) {
            static_cast<Derived&>(*this).adapterHasBecomeDefined();
        }
    }
    void memberHasBecomeUndefined(UInt) final {
        if (mpark::get<ExprRef<TupleView>>(this->op->preImageOperand)
                ->view()
                .numberUndefined == 1) {
            static_cast<Derived&>(*this).adapterHasBecomeUndefined();
        }
    }
};

template <typename FunctionMemberViewType, typename TriggerType>
struct PreImageTrigger
    : public PreImageTriggerHelper<
          FunctionMemberViewType, TriggerType,
          PreImageTrigger<FunctionMemberViewType, TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type PreImageType;
    using PreImageTriggerHelper<
        FunctionMemberViewType, TriggerType,
        PreImageTrigger<FunctionMemberViewType,
                        TriggerType>>::PreImageTriggerHelper;

    void adapterPossibleValueChange() {
        if (!this->op->locallyDefined) {
            return;
        }

        visitTriggers([&](auto& t) { t->possibleValueChange(); },
                      this->op->triggers);
    }

    void adapterValueChanged() {
        bool wasDefined = this->op->locallyDefined;
        this->op->reevaluate();
        if (wasDefined && !this->op->locallyDefined) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          this->op->triggers);
        } else if (!wasDefined && this->op->locallyDefined) {
            visitTriggers(
                [&](auto& t) {
                    t->hasBecomeDefined();
                    t->reattachTrigger();
                },
                this->op->triggers);
        } else if (this->op->locallyDefined) {
            visitTriggers(
                [&](auto& t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                this->op->triggers);
        }
    }

    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<
                      PreImageTrigger<FunctionMemberViewType, TriggerType>>(
            this->op->preImageTrigger));
        auto trigger =
            make_shared<PreImageTrigger<FunctionMemberViewType, TriggerType>>(
                this->op);
        mpark::get<ExprRef<PreImageType>>(this->op->preImageOperand)
            ->addTrigger(trigger);
        this->op->preImageTrigger = trigger;
    }
    void adapterHasBecomeDefined() {
        this->op->reevaluate();
        if (!this->op->locallyDefined) {
            return;
        }
        visitTriggers(
            [&](auto& t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            this->op->triggers);
    }

    void adapterHasBecomeUndefined() {
        if (!this->op->locallyDefined) {
            return;
        }
        this->op->locallyDefined = false;
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                      this->op->triggers);
    }
};

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::startTriggering() {
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
    }
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
        preImageTrigger = nullptr;
    }
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::stopTriggering() {
    if (preImageTrigger) {
        stopTriggeringOnChildren();
        invoke(preImageOperand, stopTriggering());
        ;
    }
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::updateVarViolations(
    UInt parentViolation, ViolationContainer& vioDesc) {
    invoke(preImageOperand,
           updateVarViolations(parentViolation, vioDesc));
    if (locallyDefined) {
        getMember()->updateVarViolations(parentViolation, vioDesc);
    } else {
        functionOperand->updateVarViolations(parentViolation, vioDesc);
    }
}

template <typename FunctionMemberViewType>
ExprRef<FunctionMemberViewType>
OpFunctionImage<FunctionMemberViewType>::deepCopySelfForUnroll(
    const ExprRef<FunctionMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpFunctionImage =
        make_shared<OpFunctionImage<FunctionMemberViewType>>(
            functionOperand->deepCopySelfForUnroll(functionOperand, iterator),
            invoke_r(preImageOperand,
                     deepCopySelfForUnroll(preImageOperand, iterator),
                     AnyExprRef));
    newOpFunctionImage->evaluated = this->evaluated;
    newOpFunctionImage->cachedIndex = cachedIndex;
    newOpFunctionImage->locallyDefined = locallyDefined;
    return newOpFunctionImage;
}

template <typename FunctionMemberViewType>
std::ostream& OpFunctionImage<FunctionMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opFunctionImage(function=";
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
bool OpFunctionImage<FunctionMemberViewType>::isUndefined() {
    return !locallyDefined;
}

template <typename FunctionMemberViewType>
bool OpFunctionImage<FunctionMemberViewType>::optimise(PathExtension path) {
    bool changeMade = false;
    changeMade |= functionOperand->optimise(path.extend( functionOperand));
    changeMade |=
        invoke(preImageOperand, optimise(path.extend( preImageOperand)));
    return changeMade;
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
