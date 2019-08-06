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
    handleTriggerAdd(trigger, includeMembers, memberIndex, *this);
}

template <typename FunctionMemberViewType>
OptionalRef<ExprRef<FunctionMemberViewType>>
OpFunctionImage<FunctionMemberViewType>::getMember() {
    if (!invoke(preImageOperand, appearsDefined())) {
        return EmptyOptional();
    }
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
    if (!invoke(preImageOperand, appearsDefined())) {
        return EmptyOptional();
    }
    debug_code(assert(cachedIndex >= 0));
    auto view = functionOperand->getViewIfDefined();
    if (!view || cachedIndex >= (Int)(*view).rangeSize()) {
        return EmptyOptional();
    }
    const auto& member =
        (*view).getRange<FunctionMemberViewType>()[cachedIndex];
    return member;
}

/*helper functions, returns empty optional if not BoolView return type otherwise
 * returns violating BoolView. */

template <typename View>
inline OptionalRef<View> emptyOrViolatingOptional() {
    return EmptyOptional();
}

template <>
inline OptionalRef<BoolView> emptyOrViolatingOptional<BoolView>() {
    return VIOLATING_BOOL_VIEW;
}

template <typename FunctionMemberViewType>
OptionalRef<FunctionMemberViewType>
OpFunctionImage<FunctionMemberViewType>::view() {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return emptyOrViolatingOptional<FunctionMemberViewType>();
    }
}

template <typename FunctionMemberViewType>
OptionalRef<const FunctionMemberViewType>
OpFunctionImage<FunctionMemberViewType>::view() const {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return emptyOrViolatingOptional<FunctionMemberViewType>();
    }
}

template <typename FunctionMemberViewType, typename TriggerType>
struct PreImageTrigger;

template <typename FunctionMemberViewType>
lib::optional<UInt> OpFunctionImage<FunctionMemberViewType>::calculateIndex()
    const {
    auto functionView = functionOperand->getViewIfDefined();
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
bool OpFunctionImage<FunctionMemberViewType>::allowForwardingOfTrigger() {
    return true;
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
bool OpFunctionImage<FunctionMemberViewType>::eventForwardedAsDefinednessChange(
    bool recalculateIndex) {
    bool wasDefined = this->appearsDefined();
    bool wasLocallyDefined = locallyDefined;
    reevaluate(recalculateIndex);
    if (!locallyDefined) {
        if (functionMemberTrigger) {
            deleteTrigger(functionMemberTrigger);
            functionMemberTrigger = nullptr;
        }
        if (memberTrigger) {
            deleteTrigger(memberTrigger);
            memberTrigger = nullptr;
        }
    } else if (!wasLocallyDefined && locallyDefined) {
        reattachFunctionMemberTrigger();
    }
    if (!is_same<BoolView, FunctionMemberViewType>::value && wasDefined &&
        !this->appearsDefined()) {
        this->notifyValueUndefined();
        return true;
    } else if (!is_same<BoolView, FunctionMemberViewType>::value &&
               !wasDefined && this->appearsDefined()) {
        this->notifyValueDefined();
        return true;
    } else if (is_same<BoolView, FunctionMemberViewType>::value &&
               wasLocallyDefined != locallyDefined) {
        this->notifyEntireValueChanged();
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
            op->notifyEntireValueChanged();
            op->reattachFunctionMemberTrigger();
        }
    }

    void valueChanged() final {
        if (!op->eventForwardedAsDefinednessChange(true)) {
            op->notifyEntireValueChanged();
            op->reattachFunctionMemberTrigger();
        }
    }

    inline void hasBecomeUndefined() {
        op->eventForwardedAsDefinednessChange(false);
    }

    void hasBecomeDefined() { op->eventForwardedAsDefinednessChange(true); }

    void reattachTrigger() final {
        deleteTrigger(op->functionOperandTrigger);
        if (op->functionMemberTrigger) {
            deleteTrigger(op->functionMemberTrigger);
        }
        if (op->memberTrigger) {
            deleteTrigger(op->memberTrigger);
        }
        auto trigger = make_shared<FunctionOperandTrigger>(op);
        op->functionOperand->addTrigger(trigger, false);
        if (op->locallyDefined) {
            op->reattachFunctionMemberTrigger();
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
    using OpFunctionImage<FunctionMemberViewType>::PreImageTriggerBase::op;
    PreImageTrigger(OpFunctionImage<FunctionMemberViewType>* op)
        : ChangeTriggerAdapter<
              TriggerType,
              PreImageTrigger<FunctionMemberViewType, TriggerType>>(
              mpark::get<ExprRef<PreImageType>>(op->preImageOperand)),
          OpFunctionImage<FunctionMemberViewType>::PreImageTriggerBase(op) {}

    ExprRef<PreImageType>& getTriggeringOperand() {
        return mpark::get<ExprRef<PreImageType>>(op->preImageOperand);
    }
    void adapterValueChanged() {
        if (!op->eventForwardedAsDefinednessChange(true)) {
            op->notifyEntireValueChanged();
            op->reattachFunctionMemberTrigger();
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
        op->eventForwardedAsDefinednessChange(true);
    }

    void adapterHasBecomeUndefined() {
        op->eventForwardedAsDefinednessChange(false);
    }
};

template <typename FunctionMemberViewType>
struct OpFunctionImage<FunctionMemberViewType>::MemberTrigger
    : public ForwardingTrigger<
          typename AssociatedTriggerType<FunctionMemberViewType>::type,
          OpFunctionImage<FunctionMemberViewType>,
          typename OpFunctionImage<FunctionMemberViewType>::MemberTrigger> {
    using ForwardingTrigger<
        typename AssociatedTriggerType<FunctionMemberViewType>::type,
        OpFunctionImage<FunctionMemberViewType>,
        typename OpFunctionImage<FunctionMemberViewType>::MemberTrigger>::
        ForwardingTrigger;
    ExprRef<FunctionMemberViewType>& getTriggeringOperand() {
        return this->op->getMember().get();
    }
    void reattachTrigger() final {
        deleteTrigger(this->op->memberTrigger);
        auto trigger = make_shared<
            typename OpFunctionImage<FunctionMemberViewType>::MemberTrigger>(
            this->op);
        this->op->getMember().get()->addTrigger(trigger);
        this->op->memberTrigger = trigger;
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
        if (locallyDefined) {
            reattachFunctionMemberTrigger();
        }
        functionOperand->startTriggering();
    }
}

template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::reattachFunctionMemberTrigger() {
    if (functionMemberTrigger) {
        deleteTrigger(functionMemberTrigger);
    }
    if (memberTrigger) {
        deleteTrigger(memberTrigger);
    }
    functionMemberTrigger = make_shared<FunctionOperandTrigger>(this);
    memberTrigger =
        make_shared<OpFunctionImage<FunctionMemberViewType>::MemberTrigger>(
            this);
    functionOperand->addTrigger(functionMemberTrigger, true, cachedIndex);
    getMember().get()->addTrigger(memberTrigger);
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
        if (functionMemberTrigger) {
            deleteTrigger(functionMemberTrigger);
        }
        if (memberTrigger) {
            deleteTrigger(memberTrigger);
        }
        preImageTrigger = nullptr;
        functionOperandTrigger = nullptr;
        functionMemberTrigger = nullptr;
        memberTrigger = nullptr;
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
    invoke(preImageOperand, updateVarViolations(vioContext, vioContainer));
    auto functionMember = getMember();
    if (locallyDefined && functionMember) {
        (*functionMember)->updateVarViolations(vioContext, vioContainer);
    } else {
        functionOperand->updateVarViolations(vioContext, vioContainer);
    }
}

template <typename FunctionMemberViewType>
ExprRef<FunctionMemberViewType>
OpFunctionImage<FunctionMemberViewType>::deepCopyForUnrollImpl(
    const ExprRef<FunctionMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpFunctionImage =
        make_shared<OpFunctionImage<FunctionMemberViewType>>(
            functionOperand->deepCopyForUnroll(functionOperand, iterator),
            invoke_r(preImageOperand,
                     deepCopyForUnroll(preImageOperand, iterator), AnyExprRef));
    newOpFunctionImage->cachedIndex = cachedIndex;
    newOpFunctionImage->locallyDefined = locallyDefined;
    return newOpFunctionImage;
}

template <typename FunctionMemberViewType>
std::ostream& OpFunctionImage<FunctionMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opFunctionImage(defined=" << this->appearsDefined()
       << ",locallyDefined=" << locallyDefined << ",value=";
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

template <typename FunctionMemberViewType>
string OpFunctionImage<FunctionMemberViewType>::getOpName() const {
    return toString(
        "OpFunctionImage<",
        TypeAsString<
            typename AssociatedValueType<FunctionMemberViewType>::type>::value,
        ">");
}
template <typename FunctionMemberViewType>
void OpFunctionImage<FunctionMemberViewType>::debugSanityCheckImpl() const {
    functionOperand->debugSanityCheck();
    invoke(preImageOperand, debugSanityCheck());
    if (!functionOperand->appearsDefined() ||
        !invoke(preImageOperand, appearsDefined())) {
        if (!is_same<BoolView, FunctionMemberViewType>::value) {
            sanityCheck(
                !this->appearsDefined(),
                "operator should be undefined as at least one operand is "
                "undefined.");
        }
        return;
    }
    auto index = calculateIndex();
    if (!index) {
        if (!is_same<BoolView, FunctionMemberViewType>::value) {
            sanityCheck(!this->appearsDefined(),
                        "Image out of bounds, operator should be undefined.");
        }
        return;
    }
    sanityCheck(this->appearsDefined(), "operator should be defined.");
    sanityEqualsCheck((Int)(*index), cachedIndex);
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
