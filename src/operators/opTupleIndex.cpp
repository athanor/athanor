#include "operators/opTupleIndex.h"
#include <iostream>
#include <memory>
#include "operators/emptyOrViolatingOptional.h"
#include "triggers/allTriggers.h"
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::addTriggerImpl(
    const shared_ptr<TupleMemberTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    handleTriggerAdd(trigger, includeMembers, memberIndex, *this);
}

template <typename TupleMemberViewType>
OptionalRef<ExprRef<TupleMemberViewType>>
OpTupleIndex<TupleMemberViewType>::getMember() {
    auto view = tupleOperand->getViewIfDefined();
    if (!view) {
        return EmptyOptional();
    }

    return mpark::get<ExprRef<TupleMemberViewType>>(
        (*view).members[indexOperand]);
}

template <typename TupleMemberViewType>
OptionalRef<const ExprRef<TupleMemberViewType>>
OpTupleIndex<TupleMemberViewType>::getMember() const {
    auto view = tupleOperand->getViewIfDefined();
    if (!view) {
        return EmptyOptional();
    }
    return mpark::get<ExprRef<TupleMemberViewType>>(
        (*view).members[indexOperand]);
}

template <typename TupleMemberViewType>
OptionalRef<TupleMemberViewType> OpTupleIndex<TupleMemberViewType>::view() {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return emptyOrViolatingOptional<TupleMemberViewType>();
    }
}
template <typename TupleMemberViewType>
OptionalRef<const TupleMemberViewType> OpTupleIndex<TupleMemberViewType>::view()
    const {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return emptyOrViolatingOptional<TupleMemberViewType>();
    }
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::reevaluate() {
    auto member = getMember();
    setAppearsDefined(member && (*member)->appearsDefined());
    exprDefined = member && (*member)->appearsDefined();
}
template <typename TupleMemberViewType>
bool OpTupleIndex<TupleMemberViewType>::allowForwardingOfTrigger() {
    return true;
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::evaluateImpl() {
    tupleOperand->evaluate();
    reevaluate();
}

template <typename TupleMemberViewType>
bool OpTupleIndex<TupleMemberViewType>::eventForwardedAsDefinednessChange() {
    bool wasDefined = exprDefined;
    reevaluate();
    if (wasDefined && !exprDefined) {
        if (is_same<BoolView, TupleMemberViewType>::value) {
            this->notifyEntireValueChanged();
        } else {
            this->notifyValueUndefined();
        }
        return true;
    } else if (!wasDefined && exprDefined) {
        if (is_same<BoolView, TupleMemberViewType>::value) {
            this->notifyEntireValueChanged();
        } else {
            this->notifyValueDefined();
        }
        return true;
    } else {
        return !exprDefined;
    }
}

template <typename TupleMemberViewType>
struct OpTupleIndex<TupleMemberViewType>::TupleOperandTrigger
    : public TupleTrigger {
    OpTupleIndex<TupleMemberViewType>* op;
    TupleOperandTrigger(OpTupleIndex<TupleMemberViewType>* op) : op(op) {}
    void memberValueChanged(UInt) {
        // ignore, already triggering on member
    }

    void memberHasBecomeUndefined(UInt index) final {
        debug_code(assert(index == op->indexOperand));
        ignoreUnused(index);
        if (!op->appearsDefined()) {
            return;
        }
        op->setAppearsDefined(false);
        op->notifyValueUndefined();
    }

    void memberHasBecomeDefined(UInt index) final {
        debug_code(assert(index == op->indexOperand));
        ignoreUnused(index);
        op->setAppearsDefined(true);
        op->notifyValueDefined();
    }

    void valueChanged() final {
        if (!op->eventForwardedAsDefinednessChange()) {
            op->notifyEntireValueChanged();
            op->reattachTupleMemberTrigger();
        }
    }

    inline void hasBecomeUndefined() {
        op->eventForwardedAsDefinednessChange();
    }

    void hasBecomeDefined() { op->eventForwardedAsDefinednessChange(); }
    void reattachTrigger() final {
        deleteTrigger(op->tupleOperandTrigger);
        if (op->tupleMemberTrigger) {
            deleteTrigger(op->tupleMemberTrigger);
        }
        if (op->memberTrigger) {
            deleteTrigger(op->memberTrigger);
        }
        auto trigger = make_shared<TupleOperandTrigger>(op);
        op->tupleOperand->addTrigger(trigger, false);
        op->reattachTupleMemberTrigger();
        op->tupleOperandTrigger = trigger;
    }
};

template <typename TupleMemberViewType>
struct OpTupleIndex<TupleMemberViewType>::MemberTrigger
    : public ForwardingTrigger<
          typename AssociatedTriggerType<TupleMemberViewType>::type,
          OpTupleIndex<TupleMemberViewType>,
          typename OpTupleIndex<TupleMemberViewType>::MemberTrigger> {
    using ForwardingTrigger<
        typename AssociatedTriggerType<TupleMemberViewType>::type,
        OpTupleIndex<TupleMemberViewType>,
        typename OpTupleIndex<TupleMemberViewType>::MemberTrigger>::
        ForwardingTrigger;
    ExprRef<TupleMemberViewType>& getTriggeringOperand() {
        return this->op->getMember().get();
    }
    void reattachTrigger() final {
        deleteTrigger(this->op->memberTrigger);
        auto trigger = make_shared<
            typename OpTupleIndex<TupleMemberViewType>::MemberTrigger>(
            this->op);
        this->op->getMember().get()->addTrigger(trigger);
        this->op->memberTrigger = trigger;
    }
};

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::startTriggeringImpl() {
    if (!tupleOperandTrigger) {
        tupleOperandTrigger =
            make_shared<OpTupleIndex<TupleMemberViewType>::TupleOperandTrigger>(
                this);
        tupleOperand->addTrigger(tupleOperandTrigger, false);
        reattachTupleMemberTrigger();
        tupleOperand->startTriggering();
    }
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::reattachTupleMemberTrigger() {
    if (tupleMemberTrigger) {
        deleteTrigger(tupleMemberTrigger);
    }
    if (memberTrigger) {
        deleteTrigger(memberTrigger);
    }
    tupleMemberTrigger = make_shared<TupleOperandTrigger>(this);
    memberTrigger =
        make_shared<OpTupleIndex<TupleMemberViewType>::MemberTrigger>(this);
    tupleOperand->addTrigger(tupleMemberTrigger, true, indexOperand);
    getMember().get()->addTrigger(memberTrigger);
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::stopTriggeringOnChildren() {
    if (tupleOperandTrigger) {
        deleteTrigger(tupleOperandTrigger);
        deleteTrigger(tupleMemberTrigger);
        deleteTrigger(memberTrigger);

        tupleOperandTrigger = nullptr;
        tupleMemberTrigger = nullptr;
        memberTrigger = nullptr;
    }
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::stopTriggering() {
    if (tupleOperandTrigger) {
        stopTriggeringOnChildren();
        tupleOperand->stopTriggering();
    }
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    auto tupleMember = getMember();
    if (tupleMember) {
        (*tupleMember)->updateVarViolations(vioContext, vioContainer);
    } else {
        tupleOperand->updateVarViolations(vioContext, vioContainer);
    }
}

template <typename TupleMemberViewType>
ExprRef<TupleMemberViewType>
OpTupleIndex<TupleMemberViewType>::deepCopyForUnrollImpl(
    const ExprRef<TupleMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpTupleIndex = make_shared<OpTupleIndex<TupleMemberViewType>>(
        tupleOperand->deepCopyForUnroll(tupleOperand, iterator), indexOperand);
    newOpTupleIndex->exprDefined = exprDefined;
    return newOpTupleIndex;
}

template <typename TupleMemberViewType>
std::ostream& OpTupleIndex<TupleMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opTupleIndex(value=";
    prettyPrint(os, this->getViewIfDefined());
    os << ",\n";

    os << "tuple=";
    tupleOperand->dumpState(os) << ",\n";
    os << "index=";
    os << indexOperand << ")";
    return os;
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->tupleOperand = findAndReplace(tupleOperand, func);
}

template <typename TupleMemberViewType>
pair<bool, ExprRef<TupleMemberViewType>>
OpTupleIndex<TupleMemberViewType>::optimiseImpl(ExprRef<TupleMemberViewType>&,
                                                PathExtension path) {
    bool optimised = false;
    auto newOp = make_shared<OpTupleIndex<TupleMemberViewType>>(tupleOperand,
                                                                indexOperand);
    AnyExprRef newOpAsExpr = ExprRef<TupleMemberViewType>(newOp);
    optimised |= optimise(newOpAsExpr, newOp->tupleOperand, path);
    return make_pair(optimised, newOp);
}

template <typename TupleMemberViewType>
string OpTupleIndex<TupleMemberViewType>::getOpName() const {
    return toString(
        "OpTupleIndex<",
        TypeAsString<
            typename AssociatedValueType<TupleMemberViewType>::type>::value,
        ">");
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::debugSanityCheckImpl() const {
    tupleOperand->debugSanityCheck();
    if (!tupleOperand->appearsDefined()) {
        sanityCheck(!this->appearsDefined(),
                    "operator should be undefined as at least one operand is "
                    "undefined.");
        return;
    }
    if (!(*getMember())->appearsDefined()) {
        sanityCheck(!this->appearsDefined(),
                    "member pointed to by index is undefined, hence this "
                    "operator should be undefined.");
        return;
    }
    sanityCheck(this->appearsDefined(), "operator should be defined.");
}
template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpTupleIndex<View>> {
    static ExprRef<View> make(ExprRef<TupleView> tuple, UInt index);
};

template <typename View>
ExprRef<View> OpMaker<OpTupleIndex<View>>::make(ExprRef<TupleView> tuple,
                                                UInt index) {
    return make_shared<OpTupleIndex<View>>(move(tuple), index);
}

#define opTupleIndexInstantiators(name)       \
    template struct OpTupleIndex<name##View>; \
    template struct OpMaker<OpTupleIndex<name##View>>;

buildForAllTypes(opTupleIndexInstantiators, );
#undef opTupleIndexInstantiators
