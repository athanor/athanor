
#include "operators/opSubstringQuantify.h"
#include <iostream>
#include <memory>
#include "operators/emptyOrViolatingOptional.h"
#include "operators/opTupleLit.h"
#include "triggers/allTriggers.h"
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename>
struct OpMaker;
template <typename>
struct OpUndefined;
template <typename View>
struct OpMaker<OpUndefined<View>> {
    static ExprRef<View> make();
};

struct OpTupleLit;
template <>
struct OpMaker<OpTupleLit> {
    static ExprRef<TupleView> make(std::vector<AnyExprRef> members);
};
// if lower and upper bounds not in the bounds of the sequence, undefined expr
// is returned.  Otherwise a tuple containing the values between lower and upper
// bound inclusive is returned.
template <typename SequenceMemberViewType>
ExprRef<TupleView> makeWindowTuple(SequenceView& sequence, Int lowerBound,
                                   Int upperBound) {
    if (lowerBound < 0 || upperBound >= (Int)sequence.numberElements()) {
        return OpMaker<OpUndefined<TupleView>>::make();
    }
    vector<AnyExprRef> tupleMembers;
    for (Int i = lowerBound; i <= upperBound; i++) {
        tupleMembers.emplace_back(
            sequence.getMembers<SequenceMemberViewType>()[i]);
    }
    auto windowTuple = OpMaker<OpTupleLit>::make(move(tupleMembers));
    windowTuple->evaluate();
    return windowTuple;
}

template <typename SequenceMemberViewType>
void OpSubstringQuantify<SequenceMemberViewType>::reevaluate() {
    this->members.template emplace<ExprRefVec<TupleView>>();
    this->silentClear();
    auto sequenceView = sequenceOperand->view();
    // using view() here instead of getViewIfDefined() as sequences can be
    // partially defined
    auto lowerBoundView = lowerBoundOperand->getViewIfDefined();
    auto upperBoundView = upperBoundOperand->getViewIfDefined();
    if (!sequenceView || !lowerBoundView || !upperBoundView) {
        this->setAppearsDefined(false);
        return;
    }
    cachedLowerBound = lowerBoundView->value - 1;
    cachedUpperBound = upperBoundView->value - 1;
    this->setAppearsDefined(true);
    for (Int i = cachedLowerBound; i <= cachedUpperBound; i++) {
        // window points backwards
        auto window = makeWindowTuple<SequenceMemberViewType>(
            *sequenceView, i - (windowSize - 1), i);
        this->addMember(this->numberElements(), window);
    }
}

template <typename SequenceMemberViewType>
void OpSubstringQuantify<SequenceMemberViewType>::evaluateImpl() {
    members.emplace<ExprRefVec<TupleView>>();
    sequenceOperand->evaluate();
    lowerBoundOperand->evaluate();
    upperBoundOperand->evaluate();
    reevaluate();
}

template <typename SequenceMemberViewType>
struct OpSubstringQuantify<SequenceMemberViewType>::SequenceOperandTrigger
    : public SequenceTrigger {
    OpSubstringQuantify<SequenceMemberViewType>* op;
    SequenceOperandTrigger(OpSubstringQuantify<SequenceMemberViewType>* op)
        : op(op) {}
    inline void valueAdded(UInt index, const AnyExprRef&) final {
        auto& sequence = op->sequenceOperand->getViewIfDefined().checkedGet(
            "Error: OpSubStringQuantify: unhandled undefined view.");
        if ((Int)index < op->cachedLowerBound) {
            handleInsertionBeforeLowerBound(sequence);
        } else if ((Int)index > op->cachedUpperBound) {
            // nothing
            return;
        } else {
            handleInsertionBetweenBounds(sequence, index);
        }
    }

    void handleInsertionBeforeLowerBound(SequenceView& sequence) {
        if (op->numberElements() == 0) {
            return;
        }
        op->removeMemberAndNotify<TupleView>(op->numberElements() - 1);
        auto newFirstMember = makeWindowTuple<SequenceMemberViewType>(
            sequence, op->cachedLowerBound - (op->windowSize - 1),
            op->cachedLowerBound);
        op->addMemberAndNotify<TupleView>(0, newFirstMember);
    }
    void handleInsertionBetweenBounds(SequenceView& sequence, Int index) {
        Int start = index;
        Int end = min(index + op->windowSize, op->cachedUpperBound + 1);
        // start is inclusive, end is exclusive
        for (Int i = start; i < end; i++) {
            Int destIndex = calcDestIndex(i);
            // position in the output sequence
            auto newWindow = makeWindowTuple<SequenceMemberViewType>(
                sequence, i - (op->windowSize - 1), i);
            newWindow->startTriggering();
            if (i != end - 1) {
                op->replaceMemberAndNotify(destIndex, newWindow);
            } else {
                op->addMemberAndNotify(destIndex, newWindow);
            }
        }
        op->removeMemberAndNotify<TupleView>(op->numberElements() - 1);
    }
    inline void valueRemoved(UInt index, const AnyExprRef&) final {
        auto& sequence = op->sequenceOperand->getViewIfDefined().checkedGet(
            "Error: OpSubStringQuantify: unhandled undefined view.");
        if ((Int)index < op->cachedLowerBound) {
            handleRemovalBeforeLowerBound(sequence);
        } else if ((Int)index > op->cachedUpperBound) {
            // nothing
            return;
        } else {
            handleRemovalBetweenBounds(sequence, index);
        }
    }

    void handleRemovalBeforeLowerBound(SequenceView& sequence) {
        if (op->numberElements() == 0) {
            return;
        }
        op->removeMemberAndNotify<TupleView>(0);
        auto newLastMember = makeWindowTuple<SequenceMemberViewType>(
            sequence, op->cachedUpperBound - (op->windowSize - 1),
            op->cachedUpperBound);
        op->addMemberAndNotify<TupleView>(op->numberElements(), newLastMember);
    }
    void handleRemovalBetweenBounds(SequenceView& sequence, Int index) {
        Int start = index;
        Int end = min(index + op->windowSize, op->cachedUpperBound + 1);
        // start is inclusive, end is exclusive
        for (Int i = start; i < end; i++) {
            Int destIndex = calcDestIndex(i);
            // position in the output sequence
            if (i != end - 1) {
                auto newWindow = makeWindowTuple<SequenceMemberViewType>(
                    sequence, i - (op->windowSize - 1), i);
                newWindow->startTriggering();
                op->replaceMemberAndNotify(destIndex, newWindow);
            } else {
                op->removeMemberAndNotify<TupleView>(destIndex);
            }
        }
        op->addMemberAndNotify(
            op->numberElements(),
            makeWindowTuple<SequenceMemberViewType>(
                sequence, op->cachedUpperBound - (op->windowSize - 1),
                op->cachedUpperBound));
    }
    void memberReplaced(UInt, const AnyExprRef&) { todoImpl(); }
    void subsequenceChanged(UInt, UInt) {}

    void memberHasBecomeUndefined(UInt) final {}

    void memberHasBecomeDefined(UInt) final {}

    // given an index in the operand sequence, calculate the index of the window
    // in the oeprator sequence
    Int calcDestIndex(Int index) { return index - op->cachedLowerBound; }

    // return a the window tuple in the operator (output) sequence based of an
    // index of a member in the operand sequence
    void positionsSwapped(UInt index1, UInt index2) final {
        auto sequenceView = op->sequenceOperand->view();
        if (!sequenceView) {
            hasBecomeUndefined();
            return;
        }
        Int sequenceSize = sequenceView->numberElements();
        auto newMember1 =
            sequenceView->template getMembers<SequenceMemberViewType>()[index1];
        auto newMember2 =
            sequenceView->template getMembers<SequenceMemberViewType>()[index2];
        Int start1 =
            max({(Int)index1, op->windowSize - 1, op->cachedLowerBound});
        // we take the max here as index1 that is less than the window size just
        // yields undefined values. also when index1 is less than
        // cachedLowerBound, it has no effect on the region around index1.
        Int end1 = min({(Int)index1 + op->windowSize, op->cachedUpperBound + 1,
                        sequenceSize});
        Int start2 =
            max({(Int)index2, op->windowSize - 1, op->cachedLowerBound});
        Int end2 = min({(Int)index2 + op->windowSize, op->cachedUpperBound + 1,
                        sequenceSize});
        // start is inclusive, end is exclusive
        auto& opMembers = op->getMembers<TupleView>();
        for (Int i = start1; i < end1; i++) {
            Int destIndex = calcDestIndex(i);
            auto window = getAs<OpTupleLit>(opMembers[destIndex]);
            // calc  index of member in tuple
            Int tupleMemberIndex = op->windowSize - 1 - (i - index1);
            window->replaceMember(tupleMemberIndex, newMember1);
            op->changeSubsequenceAndNotify<TupleView>(destIndex, destIndex + 1);
        }
        for (Int i = start2; i < end2; i++) {
            Int destIndex = calcDestIndex(i);
            auto window = getAs<OpTupleLit>(opMembers[destIndex]);
            // calc  index of member in tuple
            Int tupleMemberIndex = op->windowSize - 1 - (i - index2);
            window->replaceMember(tupleMemberIndex, newMember2);
            op->changeSubsequenceAndNotify<TupleView>(destIndex, destIndex + 1);
        }
    }

    void valueChanged() final {
        op->reevaluate();
        op->notifyEntireValueChanged();
    }

    inline void hasBecomeUndefined() { op->notifyValueUndefined(); }

    void hasBecomeDefined() {
        op->reevaluate();
        op->notifyValueDefined();
    }
    void reattachTrigger() final {
        deleteTrigger(op->sequenceOperandTrigger);
        auto trigger = make_shared<SequenceOperandTrigger>(op);
        op->sequenceOperand->addTrigger(trigger);
        op->sequenceOperandTrigger = trigger;
    }
};

template <typename SequenceMemberViewType>
struct OpSubstringQuantify<SequenceMemberViewType>::BoundsTrigger
    : public IntTrigger {
    OpSubstringQuantify<SequenceMemberViewType>* op;
    bool lower;  // true = lower, false = upper
    BoundsTrigger(OpSubstringQuantify<SequenceMemberViewType>* op, bool lower)
        : op(op), lower(lower) {}
    void valueChanged() {
        if (!op->sequenceOperand->view()) {
            if (op->appearsDefined()) {
                op->setAppearsDefined(false);
                op->notifyValueUndefined();
            }
            return;
        }
        if (lower) {
            lowerChanged();
        } else {
            upperChanged();
        }
    }
    void lowerChanged() {
        Int newLower = op->lowerBoundOperand->getViewIfDefined()
                           .checkedGet("not handling undefined here.")
                           .value -
                       1;
        auto& sequence = op->sequenceOperand->view().checkedGet(
            "Not handling undefined here.");
        while (newLower < op->cachedLowerBound) {
            --op->cachedLowerBound;
            op->addMemberAndNotify(
                0, makeWindowTuple<SequenceMemberViewType>(
                       sequence, op->cachedLowerBound - (op->windowSize - 1),
                       op->cachedLowerBound));
        }
        while (newLower > op->cachedLowerBound) {
            ++op->cachedLowerBound;
            op->removeMemberAndNotify<TupleView>(0);
        }
    }

    void upperChanged() {
        Int newUpper = op->upperBoundOperand->getViewIfDefined()
                           .checkedGet("not handling undefined here.")
                           .value -
                       1;
        auto& sequence = op->sequenceOperand->view().checkedGet(
            "Not handling undefined here.");
        while (newUpper > op->cachedUpperBound) {
            ++op->cachedUpperBound;
            op->addMemberAndNotify(
                op->numberElements(),
                makeWindowTuple<SequenceMemberViewType>(
                    sequence, op->cachedUpperBound - (op->windowSize - 1),
                    op->cachedUpperBound));
        }
        while (newUpper < op->cachedUpperBound) {
            --op->cachedUpperBound;
            op->removeMemberAndNotify<TupleView>(op->numberElements() - 1);
        }
    }

    void reattachTrigger() final {
        auto& triggerToReplace =
            (lower) ? op->lowerBoundTrigger : op->upperBoundTrigger;
        deleteTrigger(triggerToReplace);

        auto trigger = make_shared<
            OpSubstringQuantify<SequenceMemberViewType>::BoundsTrigger>(op,
                                                                        lower);
        auto& operand = (lower) ? op->lowerBoundOperand : op->upperBoundOperand;
        operand->addTrigger(trigger);
        triggerToReplace = trigger;
    }
    void hasBecomeDefined() {
        op->reevaluate();
        op->notifyValueDefined();
    }

    void hasBecomeUndefined() { op->notifyValueUndefined(); }
};

template <typename SequenceMemberViewType>
void OpSubstringQuantify<SequenceMemberViewType>::startTriggeringImpl() {
    if (!sequenceOperandTrigger) {
        sequenceOperandTrigger = make_shared<OpSubstringQuantify<
            SequenceMemberViewType>::SequenceOperandTrigger>(this);
        sequenceOperand->addTrigger(sequenceOperandTrigger);
        lowerBoundTrigger = make_shared<
            OpSubstringQuantify<SequenceMemberViewType>::BoundsTrigger>(this,
                                                                        true);
        lowerBoundOperand->addTrigger(lowerBoundTrigger);
        upperBoundTrigger = make_shared<
            OpSubstringQuantify<SequenceMemberViewType>::BoundsTrigger>(this,
                                                                        false);
        upperBoundOperand->addTrigger(upperBoundTrigger);
        sequenceOperand->startTriggering();
        lowerBoundOperand->startTriggering();
        upperBoundOperand->startTriggering();
    }
}

template <typename SequenceMemberViewType>
void OpSubstringQuantify<SequenceMemberViewType>::stopTriggeringOnChildren() {
    if (sequenceOperandTrigger) {
        deleteTrigger(sequenceOperandTrigger);
        deleteTrigger(sequenceOperandTrigger);
        deleteTrigger(lowerBoundTrigger);
        deleteTrigger(upperBoundTrigger);
        sequenceOperandTrigger = nullptr;
        lowerBoundTrigger = nullptr;
        upperBoundTrigger = nullptr;
    }
}

template <typename SequenceMemberViewType>
void OpSubstringQuantify<SequenceMemberViewType>::stopTriggering() {
    if (sequenceOperandTrigger) {
        stopTriggeringOnChildren();
        sequenceOperand->stopTriggering();
        lowerBoundOperand->stopTriggering();
        upperBoundOperand->stopTriggering();
    }
}

template <typename SequenceMemberViewType>
void OpSubstringQuantify<SequenceMemberViewType>::updateVarViolationsImpl(
    const ViolationContext&, ViolationContainer&) {}

template <typename SequenceMemberViewType>
ExprRef<SequenceView>
OpSubstringQuantify<SequenceMemberViewType>::deepCopyForUnrollImpl(
    const ExprRef<SequenceView>&, const AnyIterRef& iterator) const {
    auto newOp = make_shared<OpSubstringQuantify<SequenceMemberViewType>>(
        sequenceOperand->deepCopyForUnroll(sequenceOperand, iterator),
        lowerBoundOperand->deepCopyForUnroll(lowerBoundOperand, iterator),
        upperBoundOperand->deepCopyForUnroll(upperBoundOperand, iterator),
        windowSize);
    return newOp;
}

template <typename SequenceMemberViewType>
std::ostream& OpSubstringQuantify<SequenceMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opSubstringQuantify(value=" << this->getViewIfDefined() << ",";
    os << "windowSize=" << windowSize << ",\n";
    os << "sequence=";
    sequenceOperand->dumpState(os) << ",\n";
    os << "lowerBoundOperand=";
    lowerBoundOperand->dumpState(os) << ",\n";
    os << "upperBoundOperand=";
    upperBoundOperand->dumpState(os) << ")";
    return os;
}

template <typename SequenceMemberViewType>
void OpSubstringQuantify<SequenceMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func, PathExtension path) {
    this->sequenceOperand = findAndReplace(sequenceOperand, func, path);
    this->lowerBoundOperand = findAndReplace(lowerBoundOperand, func, path);
    this->upperBoundOperand = findAndReplace(upperBoundOperand, func, path);
}

template <typename SequenceMemberViewType>
pair<bool, ExprRef<SequenceView>>
OpSubstringQuantify<SequenceMemberViewType>::optimiseImpl(
    ExprRef<SequenceView>&, PathExtension path) {
    bool optimised = false;
    auto newOp = make_shared<OpSubstringQuantify<SequenceMemberViewType>>(
        sequenceOperand, lowerBoundOperand, upperBoundOperand, windowSize);
    AnyExprRef newOpAsExpr = ExprRef<SequenceView>(newOp);
    optimised |= optimise(newOpAsExpr, newOp->sequenceOperand, path);
    optimised |= optimise(newOpAsExpr, newOp->lowerBoundOperand, path);
    optimised |= optimise(newOpAsExpr, newOp->upperBoundOperand, path);
    return make_pair(optimised, newOp);
}

template <typename SequenceMemberViewType>
string OpSubstringQuantify<SequenceMemberViewType>::getOpName() const {
    return toString(
        "OpSubstringQuantify<",
        TypeAsString<
            typename AssociatedValueType<SequenceMemberViewType>::type>::value,
        ">");
}

template <typename SequenceMemberViewType>
void OpSubstringQuantify<SequenceMemberViewType>::debugSanityCheckImpl() const {
    sequenceOperand->debugSanityCheck();
    lowerBoundOperand->debugSanityCheck();
    upperBoundOperand->debugSanityCheck();
    standardSanityChecksForThisType();
    auto lowerBoundView = lowerBoundOperand->getViewIfDefined();
    if (!lowerBoundOperand) {
        sanityCheck(!this->appearsDefined(),
                    "Should not be defined because lower bound is undefined.");
        return;
    }
    auto upperBoundView = upperBoundOperand->getViewIfDefined();
    if (!upperBoundView) {
        sanityCheck(!this->appearsDefined(),
                    "should be undefined as upper bound is undefined.");
        return;
    }
    auto sequenceView = sequenceOperand->view();
    // using view instead of getViewIfDefined as sequences can be partially
    // defined
    if (!sequenceView) {
        sanityCheck(
            !this->appearsDefined(),
            "should be defined as sequence operand is entirely undefined.");
        return;
    }
    sanityEqualsCheck(lowerBoundView->value - 1, cachedLowerBound);
    sanityEqualsCheck(upperBoundView->value - 1, cachedUpperBound);
    sanityEqualsCheck(cachedUpperBound - cachedLowerBound + 1,
                      (Int)numberElements());
    UInt checkNumberUndefined = 0;
    for (Int i = cachedLowerBound; i <= cachedUpperBound; i++) {
        auto tuple = makeWindowTuple<SequenceMemberViewType>(
            *sequenceView, i - (windowSize - 1), i);
        Int destIndex = i - cachedLowerBound;
        if (!tuple->appearsDefined()) {
            sanityCheck(!getMembers<TupleView>()[destIndex]->appearsDefined(),
                        toString("member with dest index ", destIndex,
                                 " should be  undefined."));
            ++checkNumberUndefined;
        } else {
            auto windowView =
                getMembers<TupleView>()[destIndex]->getViewIfDefined();
            sanityCheck(windowView,
                        toString("member with dest index ", destIndex,
                                 " should be  defined."));
            auto checkWindowView = tuple->getViewIfDefined();
            sanityCheck(
                checkWindowView,
                toString("Something funny, the check window  with dest index ",
                         destIndex, " should be defined."));
            sanityEqualsCheck(getValueHash(*checkWindowView),
                              getValueHash(*windowView));
        }
    }
    sanityEqualsCheck(checkNumberUndefined, this->numberUndefined);
}

template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpSubstringQuantify<View>> {
    static ExprRef<SequenceView> make(ExprRef<SequenceView> sequence,
                                      ExprRef<IntView> lowerBound,
                                      ExprRef<IntView> upperBound,
                                      size_t windowSize);
};

template <typename View>
ExprRef<SequenceView> OpMaker<OpSubstringQuantify<View>>::make(
    ExprRef<SequenceView> sequence, ExprRef<IntView> lowerBoundOperand,
    ExprRef<IntView> upperBoundOperand, size_t windowSize) {
    return make_shared<OpSubstringQuantify<View>>(
        move(sequence), move(lowerBoundOperand), move(upperBoundOperand),
        windowSize);
}

#define opSubstringQuantifyInstantiators(name)       \
    template struct OpSubstringQuantify<name##View>; \
    template struct OpMaker<OpSubstringQuantify<name##View>>;

buildForAllTypes(opSubstringQuantifyInstantiators, );
#undef opSubstringQuantifyInstantiators
