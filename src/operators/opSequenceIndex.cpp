#include "operators/opSequenceIndex.h"
#include <iostream>
#include <memory>
#include "operators/emptyOrViolatingOptional.h"
#include "triggers/allTriggers.h"
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::addTriggerImpl(
    const shared_ptr<SequenceMemberTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    handleTriggerAdd(trigger, includeMembers, memberIndex, *this);
}

template <typename SequenceMemberViewType>
OptionalRef<ExprRef<SequenceMemberViewType>>
OpSequenceIndex<SequenceMemberViewType>::getMember() {
    debug_code(assert(indexOperand->appearsDefined()));
    auto view = sequenceOperand->view();
    if (!view || cachedIndex < 0 ||
        cachedIndex >= (Int)(*view).numberElements()) {
        return EmptyOptional();
    }

    auto& member = (*view).getMembers<SequenceMemberViewType>()[cachedIndex];
    return member;
}

template <typename SequenceMemberViewType>
OptionalRef<const ExprRef<SequenceMemberViewType>>
OpSequenceIndex<SequenceMemberViewType>::getMember() const {
    debug_code(assert(indexOperand->appearsDefined()));
    auto view = sequenceOperand->view();
    if (!view || cachedIndex < 0 ||
        cachedIndex >= (Int)(*view).numberElements()) {
        return EmptyOptional();
    }
    const auto& member =
        (*view).getMembers<SequenceMemberViewType>()[cachedIndex];
    return member;
}

template <typename SequenceMemberViewType>
OptionalRef<SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::view() {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return emptyOrViolatingOptional<SequenceMemberViewType>();
    }
}

template <typename SequenceMemberViewType>
OptionalRef<const SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::view() const {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return emptyOrViolatingOptional<SequenceMemberViewType>();
    }
}

template <typename SequenceMemberViewType>
bool OpSequenceIndex<SequenceMemberViewType>::allowForwardingOfTrigger() {
    return true;
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::reevaluate() {
    auto sequenceView = sequenceOperand->view();
    auto indexView = indexOperand->getViewIfDefined();

    if (!sequenceView || !indexView) {
        locallyDefined = false;
        this->setAppearsDefined(false);
        return;
    }
    cachedIndex = indexView->value - 1;
    locallyDefined =
        cachedIndex >= 0 && cachedIndex < (Int)sequenceView->numberElements();
    setAppearsDefined(locallyDefined && getMember().get()->appearsDefined());
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::evaluateImpl() {
    sequenceOperand->evaluate();
    indexOperand->evaluate();
    reevaluate();
}

template <typename SequenceMemberViewType>
bool OpSequenceIndex<
    SequenceMemberViewType>::eventForwardedAsDefinednessChange() {
    bool wasDefined = this->appearsDefined();
    bool wasLocallyDefined = locallyDefined;
    reevaluate();
    if (!locallyDefined) {
        if (sequenceMemberTrigger) {
            deleteTrigger(sequenceMemberTrigger);
            sequenceMemberTrigger = nullptr;
        }
        if (memberTrigger) {
            deleteTrigger(memberTrigger);
            memberTrigger = nullptr;
        }
    } else if (!wasLocallyDefined && locallyDefined) {
        reattachSequenceMemberTrigger();
    }
    if (wasDefined && !this->appearsDefined()) {
        this->notifyValueUndefined();
        return true;
    } else if (!wasDefined && this->appearsDefined()) {
        this->notifyValueDefined();
        return true;
    } else if (is_same<BoolView, SequenceMemberViewType>::value &&
               wasLocallyDefined != locallyDefined) {
        this->notifyEntireValueChanged();
        return true;
    } else {
        return !this->appearsDefined();
    }
}

template <typename SequenceMemberViewType>
struct OpSequenceIndex<SequenceMemberViewType>::SequenceOperandTrigger
    : public SequenceTrigger {
    OpSequenceIndex<SequenceMemberViewType>* op;
    SequenceOperandTrigger(OpSequenceIndex<SequenceMemberViewType>* op)
        : op(op) {}
    inline void valueAdded(UInt index, const AnyExprRef&) final {
        if (!op->eventForwardedAsDefinednessChange() &&
            (Int)index <= op->cachedIndex) {
            op->notifyEntireValueChanged();
            op->reattachSequenceMemberTrigger();
        }
    }

    inline void valueRemoved(UInt index, const AnyExprRef&) {
        if (!op->locallyDefined) {
            return;
        }
        if (!op->eventForwardedAsDefinednessChange() &&
            (Int)index <= op->cachedIndex) {
            op->notifyEntireValueChanged();
            op->reattachSequenceMemberTrigger();
        }
    }

    void subsequenceChanged(UInt, UInt) {
        // ignore, already triggering on member
    }

    void memberHasBecomeUndefined(UInt index) final {
        debug_code(assert((!is_same<BoolView, SequenceMemberViewType>::value)));
        if (!op->appearsDefined() || (Int)index != op->cachedIndex) {
            return;
        }
        op->setAppearsDefined(false);
        op->notifyValueUndefined();
    }

    void memberHasBecomeDefined(UInt index) final {
        debug_code(assert((!is_same<BoolView, SequenceMemberViewType>::value)));
        if (!op->locallyDefined || (Int)index != op->cachedIndex) {
            return;
        }
        op->setAppearsDefined(true);
        op->notifyValueDefined();
    }

    void positionsSwapped(UInt index1, UInt index2) final {
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
        if (!op->eventForwardedAsDefinednessChange()) {
            op->notifyEntireValueChanged();
            op->reattachSequenceMemberTrigger();
        }
    }

    void valueChanged() final {
        if (!op->eventForwardedAsDefinednessChange()) {
            op->notifyEntireValueChanged();
            op->reattachSequenceMemberTrigger();
        }
    }

    inline void hasBecomeUndefined() {
        op->eventForwardedAsDefinednessChange();
    }

    void hasBecomeDefined() { op->eventForwardedAsDefinednessChange(); }
    void reattachTrigger() final {
        deleteTrigger(op->sequenceOperandTrigger);
        if (op->sequenceMemberTrigger) {
            deleteTrigger(op->sequenceMemberTrigger);
        }
        if (op->memberTrigger) {
            deleteTrigger(op->memberTrigger);
        }
        auto trigger = make_shared<SequenceOperandTrigger>(op);
        op->sequenceOperand->addTrigger(trigger, false);
        if (op->locallyDefined) {
            op->reattachSequenceMemberTrigger();
        }
        op->sequenceOperandTrigger = trigger;
    }
};

template <typename SequenceMemberViewType>
struct OpSequenceIndex<SequenceMemberViewType>::IndexTrigger
    : public IntTrigger {
    OpSequenceIndex<SequenceMemberViewType>* op;
    IndexTrigger(OpSequenceIndex<SequenceMemberViewType>* op) : op(op) {}
    void valueChanged() {
        if (!op->eventForwardedAsDefinednessChange()) {
            op->notifyEntireValueChanged();
            op->reattachSequenceMemberTrigger();
        }
    }
    void reattachTrigger() final {
        deleteTrigger(op->indexTrigger);
        auto trigger =
            make_shared<OpSequenceIndex<SequenceMemberViewType>::IndexTrigger>(
                op);
        op->indexOperand->addTrigger(trigger);
        op->indexTrigger = trigger;
    }
    void hasBecomeDefined() { op->eventForwardedAsDefinednessChange(); }

    void hasBecomeUndefined() { op->eventForwardedAsDefinednessChange(); }
};

template <typename SequenceMemberViewType>
struct OpSequenceIndex<SequenceMemberViewType>::MemberTrigger
    : public ForwardingTrigger<
          typename AssociatedTriggerType<SequenceMemberViewType>::type,
          OpSequenceIndex<SequenceMemberViewType>,
          typename OpSequenceIndex<SequenceMemberViewType>::MemberTrigger> {
    using ForwardingTrigger<
        typename AssociatedTriggerType<SequenceMemberViewType>::type,
        OpSequenceIndex<SequenceMemberViewType>,
        typename OpSequenceIndex<SequenceMemberViewType>::MemberTrigger>::
        ForwardingTrigger;
    ExprRef<SequenceMemberViewType>& getTriggeringOperand() {
        return this->op->getMember().get();
    }
    void reattachTrigger() final {
        deleteTrigger(this->op->memberTrigger);
        auto trigger = make_shared<
            typename OpSequenceIndex<SequenceMemberViewType>::MemberTrigger>(
            this->op);
        this->op->getMember().get()->addTrigger(trigger);
        this->op->memberTrigger = trigger;
    }
};

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::startTriggeringImpl() {
    if (!indexTrigger) {
        indexTrigger =
            make_shared<OpSequenceIndex<SequenceMemberViewType>::IndexTrigger>(
                this);
        indexOperand->addTrigger(indexTrigger);
        indexOperand->startTriggering();

        sequenceOperandTrigger = make_shared<
            OpSequenceIndex<SequenceMemberViewType>::SequenceOperandTrigger>(
            this);
        sequenceOperand->addTrigger(sequenceOperandTrigger, false);
        if (locallyDefined) {
            reattachSequenceMemberTrigger();
        }
        sequenceOperand->startTriggering();
    }
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::reattachSequenceMemberTrigger() {
    if (sequenceMemberTrigger) {
        deleteTrigger(sequenceMemberTrigger);
    }
    if (memberTrigger) {
        deleteTrigger(memberTrigger);
    }
    sequenceMemberTrigger = make_shared<SequenceOperandTrigger>(this);
    sequenceOperand->addTrigger(sequenceMemberTrigger, true, cachedIndex);
    memberTrigger = make_shared<
        typename OpSequenceIndex<SequenceMemberViewType>::MemberTrigger>(this);
    getMember().get()->addTrigger(memberTrigger);
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::stopTriggeringOnChildren() {
    if (indexTrigger) {
        deleteTrigger(indexTrigger);
        deleteTrigger(sequenceOperandTrigger);
        if (sequenceMemberTrigger) {
            deleteTrigger(sequenceMemberTrigger);
        }
        if (memberTrigger) {
            deleteTrigger(memberTrigger);
            ;
        }
        indexTrigger = nullptr;
        sequenceOperandTrigger = nullptr;
        sequenceMemberTrigger = nullptr;
        memberTrigger = nullptr;
    }
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::stopTriggering() {
    if (indexTrigger) {
        stopTriggeringOnChildren();
        indexOperand->stopTriggering();
        sequenceOperand->stopTriggering();
    }
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    indexOperand->updateVarViolations(vioContext, vioContainer);
    auto sequenceMember = getMember();
    if (locallyDefined && sequenceMember) {
        (*sequenceMember)->updateVarViolations(vioContext, vioContainer);
    } else {
        sequenceOperand->updateVarViolations(vioContext, vioContainer);
    }
}

template <typename SequenceMemberViewType>
ExprRef<SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::deepCopyForUnrollImpl(
    const ExprRef<SequenceMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpSequenceIndex =
        make_shared<OpSequenceIndex<SequenceMemberViewType>>(
            sequenceOperand->deepCopyForUnroll(sequenceOperand, iterator),
            indexOperand->deepCopyForUnroll(indexOperand, iterator));
    newOpSequenceIndex->cachedIndex = cachedIndex;
    newOpSequenceIndex->locallyDefined = locallyDefined;
    return newOpSequenceIndex;
}

template <typename SequenceMemberViewType>
std::ostream& OpSequenceIndex<SequenceMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opSequenceIndex(defined=" << this->appearsDefined()
       << ",value=" << this->getViewIfDefined() << ",";

    os << ",\n";

    os << "sequence=";
    sequenceOperand->dumpState(os) << ",\n";
    os << "index=";
    indexOperand->dumpState(os) << ")";
    return os;
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->sequenceOperand = findAndReplace(sequenceOperand, func);
    this->indexOperand = findAndReplace(indexOperand, func);
}

template <typename SequenceMemberViewType>
pair<bool, ExprRef<SequenceMemberViewType>>
OpSequenceIndex<SequenceMemberViewType>::optimiseImpl(
    ExprRef<SequenceMemberViewType>&, PathExtension path) {
    bool optimised = false;
    auto newOp = make_shared<OpSequenceIndex<SequenceMemberViewType>>(
        sequenceOperand, indexOperand);
    AnyExprRef newOpAsExpr = ExprRef<SequenceMemberViewType>(newOp);
    optimised |= optimise(newOpAsExpr, newOp->sequenceOperand, path);
    optimised |= optimise(newOpAsExpr, newOp->indexOperand, path);
    return make_pair(optimised, newOp);
}

template <typename SequenceMemberViewType>
string OpSequenceIndex<SequenceMemberViewType>::getOpName() const {
    return toString(
        "OpSequenceIndex<",
        TypeAsString<
            typename AssociatedValueType<SequenceMemberViewType>::type>::value,
        ">");
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::debugSanityCheckImpl() const {
    sequenceOperand->debugSanityCheck();
    indexOperand->debugSanityCheck();
    auto indexView = indexOperand->getViewIfDefined();
    auto sequenceView = sequenceOperand->view();
    if (!indexView || !sequenceView) {
        sanityCheck((is_same<BoolView, SequenceMemberViewType>::value ||
                     !this->appearsDefined()),
                    "operands are undefined, operator should be undefined.");
        return;
    } else if (indexView->value - 1 < 0 ||
               indexView->value - 1 >= (Int)sequenceView->numberElements()) {
        sanityCheck((is_same<BoolView, SequenceMemberViewType>::value ||
                     !this->appearsDefined()),
                    "index out of bounds, operator should be undefined.");
        return;
    } else {
        sanityEqualsCheck((Int)indexView->value - 1, cachedIndex);
        auto member = getMember();
        sanityCheck(member, "member returned empty optional.");
        if (!(*member)->appearsDefined()) {
            sanityCheck(!this->appearsDefined(),
                        "member pointed to by index is undefined, hence this "
                        "operator should be undefined.");
            return;
        } else {
            sanityCheck(this->appearsDefined(), "operator should be defined.");
        }
    }
}

template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpSequenceIndex<View>> {
    static ExprRef<View> make(ExprRef<SequenceView> sequence,
                              ExprRef<IntView> index);
};

template <typename View>
ExprRef<View> OpMaker<OpSequenceIndex<View>>::make(
    ExprRef<SequenceView> sequence, ExprRef<IntView> index) {
    return make_shared<OpSequenceIndex<View>>(move(sequence), move(index));
}

#define opSequenceIndexInstantiators(name)       \
    template struct OpSequenceIndex<name##View>; \
    template struct OpMaker<OpSequenceIndex<name##View>>;

buildForAllTypes(opSequenceIndexInstantiators, );
#undef opSequenceIndexInstantiators
