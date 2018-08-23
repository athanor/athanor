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
void OpFunctionImage<FunctionMemberViewType>::addTriggerImpl(
    const shared_ptr<FunctionMemberTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    triggers.emplace_back(getTriggerBase(trigger));
    if (locallyDefined) {
        getMember()->addTrigger(trigger, includeMembers, memberIndex);
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
void OpFunctionImage<FunctionMemberViewType>::reevaluate(
    bool recalculateCachedIndex) {
    if (invoke(preImageOperand, isUndefined())) {
        locallyDefined = false;
        defined = false;
        return;
    }
    if (recalculateCachedIndex) {
        auto boolIndexPair = mpark::visit(
            [&](auto& preImageOperand) {
                return functionOperand->view().domainToIndex(
                    preImageOperand->view());
            },
            preImageOperand);
        if (!boolIndexPair.first) {
            locallyDefined = false;
            defined = false;
            return;
        } else {
            locallyDefined = true;
            cachedIndex = boolIndexPair.second;
        }
    }
    defined = locallyDefined && !getMember()->isUndefined();
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
      defined(move(other.defined)),
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
    bool wasDefined = defined;
    bool wasLocallyDefined = locallyDefined;
    reevaluate(recalculateIndex);
    if (!wasLocallyDefined && locallyDefined) {
        reattachFunctionMemberTrigger(true);
    }
    if (wasDefined && !defined) {
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, triggers,
                      true);
        return true;
    } else if (!wasDefined && defined) {
        visitTriggers(
            [&](auto& t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            triggers, true);
        return true;
    } else {
        return !defined;
    }
}

template <typename FunctionMemberViewType>
struct OpFunctionImage<FunctionMemberViewType>::FunctionOperandTrigger
    : public FunctionTrigger {
    OpFunctionImage<FunctionMemberViewType>* op;
    FunctionOperandTrigger(OpFunctionImage<FunctionMemberViewType>* op)
        : op(op) {}

    void possibleImageChange(UInt) {}
    void possibleImageChange(const std::vector<UInt>&) {}
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
        if (op->defined) {
            op->cachedIndex = index2;
            visitTriggers([&](auto& t) { t->possibleValueChange(); },
                          op->triggers);
            op->cachedIndex = index1;
        }
        if (!op->eventForwardedAsDefinednessChange(false)) {
            visitTriggers(
                [&](auto& t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
            op->reattachFunctionMemberTrigger(true);
        }
    }

    void possibleValueChange() final {
        if (!op->defined) {
            return;
        }

        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
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
        if (!op->defined) {
            return;
        }
        op->defined = false;
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers);
    }

    void hasBecomeDefined() {
        op->reevaluate(true);
        if (!op->defined) {
            return;
        }
        op->reattachFunctionMemberTrigger(true);
        visitTriggers(
            [&](auto& t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            op->triggers);
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
        if (!this->op->eventForwardedAsDefinednessChange(true)) {
            visitTriggers(
                [&](auto& t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                this->op->triggers);
            this->op->reattachFunctionMemberTrigger(true);
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
        if (!this->op->defined) {
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
        if (!this->op->defined) {
            return;
        }
        this->op->locallyDefined = false;
        this->op->defined = false;
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                      this->op->triggers);
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
    if (locallyDefined) {
        getMember()->updateVarViolations(vioContext, vioContainer);
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
    newOpFunctionImage->defined = defined;
    return newOpFunctionImage;
}

template <typename FunctionMemberViewType>
std::ostream& OpFunctionImage<FunctionMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opFunctionImage(value=";
    if (defined) {
        prettyPrint(os, getMember()->view());
    } else {
        os << "undefined";
    }
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
bool OpFunctionImage<FunctionMemberViewType>::isUndefined() {
    return !defined;
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
