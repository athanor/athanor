#include "operators/opFlattenOneLevel.h"
#include <iostream>
#include <memory>
#include "types/sequence.h"
#include "utils/ignoreUnused.h"

using namespace std;
const string OP_FLATTEN_DEFAULT_ERROR =
    "View appears to be undefined.  OpFlattenOneLevel does not handle this "
    "case yet.\n";
template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::assertValidStartingIndices() const {
    auto& innerSequences = operand->view()
                               .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                               .getMembers<SequenceView>();
    UInt total = 0;
    bool success = true;
    if (innerSequences.size() != startingIndices.size()) {
        success = false;
        myCerr << "Error: startingIndices not same length as number of inner "
                  "sequences.\n";
    }
    if (success) {
        for (size_t i = 0; i < innerSequences.size(); i++) {
            if (startingIndices[i] != total) {
                success = false;
                myCerr << "Error: expected starting index at position " << i
                       << " to be " << total;
                break;
            }
            total += innerSequences[i]
                         ->view()
                         .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                         .numberElements();
        }
    }
    if (!success) {
        myCerr << "\nOperand sequences:\n";
        for (auto& innerSequence : innerSequences) {
            prettyPrint(myCerr, innerSequence->view().checkedGet(
                                    OP_FLATTEN_DEFAULT_ERROR))
                << endl;
        }
        myCerr << "startingIndices: " << startingIndices << endl;
        myCerr << "Flattened value: ";
        prettyPrint(myCerr, this->view().checkedGet(OP_FLATTEN_DEFAULT_ERROR))
            << endl;
        assert(false);
        myAbort();
    }
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::reevaluate() {
    auto& operandMembers = operand->view()
                               .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                               .getMembers<SequenceView>();
    startingIndices.resize(operandMembers.size());
    silentClear();
    for (size_t i = 0; i < operandMembers.size(); i++) {
        auto& innerSequenceMembers = operandMembers[i]
                                         ->view()
                                         .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                                         .getMembers<SequenceInnerType>();
        startingIndices[i] = numberElements();
        for (auto& member : innerSequenceMembers) {
            addMember(numberElements(), member);
        }
    }
}
template <typename SequenceInnerType>
struct OpFlattenOneLevel<SequenceInnerType>::InnerSequenceTrigger
    : public SequenceTrigger {
    OpFlattenOneLevel<SequenceInnerType>* op;
    UInt index;

    InnerSequenceTrigger(OpFlattenOneLevel<SequenceInnerType>* op, UInt index)
        : op(op), index(index) {}
    inline void valueRemoved(UInt indexOfRemoved, const AnyExprRef&) {
        op->removeMemberAndNotify<SequenceInnerType>(
            op->startingIndices[index] + indexOfRemoved);
        for (size_t i = index + 1; i < op->startingIndices.size(); i++) {
            --op->startingIndices[i];
        }
        debug_code(op->assertValidStartingIndices());
    }
    inline void valueAdded(UInt indexOfAdded, const AnyExprRef& member) final {
        op->addMemberAndNotify(op->startingIndices[index] + indexOfAdded,
                               lib::get<ExprRef<SequenceInnerType>>(member));

        for (size_t i = index + 1; i < op->startingIndices.size(); i++) {
            ++op->startingIndices[i];
        }
        debug_code(op->assertValidStartingIndices());
    }

    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        op->changeSubsequenceAndNotify<SequenceInnerType>(
            op->startingIndices[index] + startIndex,
            op->startingIndices[index] + endIndex);
    }
    void memberReplaced(UInt sourceIndex, const AnyExprRef& oldMember) final {
        auto& sourceSequence = op->operand->view()
                                   .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                                   .template getMembers<SequenceView>()[index]
                                   ->view()
                                   .checkedGet(OP_FLATTEN_DEFAULT_ERROR);
        UInt destIndex = op->startingIndices[index] + sourceIndex;
        op->getMembers<SequenceInnerType>()[destIndex] =
            sourceSequence
                .template getMembers<SequenceInnerType>()[sourceIndex];
        op->notifyMemberReplaced(destIndex, oldMember);
    }
    inline void positionsSwapped(UInt index1, UInt index2) final {
        op->swapAndNotify<SequenceInnerType>(
            op->startingIndices[index] + index1,
            op->startingIndices[index] + index2);
    }
    inline void memberHasBecomeDefined(UInt) { todoImpl(); }
    inline void memberHasBecomeUndefined(UInt) { todoImpl(); }
    void hasBecomeUndefined() {}
    void hasBecomeDefined() {}
    void reattachTrigger() {
        deleteTrigger(op->innerSequenceTriggers[index]);
        auto trigger = make_shared<
            OpFlattenOneLevel<SequenceInnerType>::InnerSequenceTrigger>(op,
                                                                        index);
        op->operand->view()
            .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
            .template getMembers<SequenceView>()[index]
            ->addTrigger(trigger);
        op->innerSequenceTriggers[index] = trigger;
    }
    void valueChanged() { todoImpl(); }
};

template <typename SequenceInnerType>
struct OpFlattenOneLevel<SequenceInnerType>::OperandTrigger
    : public SequenceTrigger {
    OpFlattenOneLevel<SequenceInnerType>* op;
    OperandTrigger(OpFlattenOneLevel<SequenceInnerType>* op) : op(op) {}

    void valueChanged() {
        op->reevaluate();
        op->reattachAllInnerSequenceTriggers(true);
        op->cachedHashTotal.invalidate();
        op->notifyEntireValueChanged();
    }

    void hasBecomeUndefined() final { todoImpl(); }
    void hasBecomeDefined() final { todoImpl(); }

    inline void correctInnerSequenceTriggerIndices(UInt index) {
        for (size_t i = index; i < op->innerSequenceTriggers.size(); i++) {
            op->innerSequenceTriggers[i]->index = i;
        }
    }

    UInt shiftStartingIndicesDown(UInt indexBeingRemoved) {
        UInt diff =
            (indexBeingRemoved < op->startingIndices.size() - 1)
                ? op->startingIndices[indexBeingRemoved + 1] -
                      op->startingIndices[indexBeingRemoved]
                : op->numberElements() - op->startingIndices[indexBeingRemoved];
        for (size_t i = indexBeingRemoved; i < op->startingIndices.size() - 1;
             i++) {
            if (op->startingIndices[i] != 0) {
                op->startingIndices[i] -= diff;
            }
        }
        op->startingIndices.pop_back();
        return diff;
    }

    void valueRemoved(UInt index, const AnyExprRef&) final {
        UInt distanceShifted = shiftStartingIndicesDown(index);
        // should be optimised later with a new type of sequence trigger
        UInt startIndex =
            (op->startingIndices.empty()) ? 0 : op->startingIndices[index];
        UInt endIndex = startIndex + distanceShifted;
        for (size_t i = endIndex; i > startIndex; i--) {
            op->removeMemberAndNotify<SequenceInnerType>(i - 1);
        }
        deleteTrigger(op->innerSequenceTriggers[index]);
        op->innerSequenceTriggers.erase(op->innerSequenceTriggers.begin() +
                                        index);
        correctInnerSequenceTriggerIndices(index);
        debug_code(op->assertValidStartingIndices());
    }

    void reattachTrigger() final {
        auto trigger = make_shared<OperandTrigger>(op);
        deleteTrigger(op->operandTrigger);
        this->op->operand->addTrigger(trigger);
        op->reattachAllInnerSequenceTriggers(true);
        op->operandTrigger = move(trigger);
    }

    void shiftStartingIndicesUp(UInt index, UInt shiftAmount) {
        if (op->startingIndices.empty()) {
            op->startingIndices.emplace_back(0);
            return;
        }
        op->startingIndices.emplace_back(op->numberElements());
        // remember op->numberElements() is size of flattened sequence.
        // this number is replaced in the for loop below anyways.
        for (size_t i = op->startingIndices.size(); i > index + 1; i--) {
            op->startingIndices[i - 1] = op->startingIndices[i - 2];
            op->startingIndices[i - 1] += shiftAmount;
        }
    }
    void valueAdded(UInt index, const AnyExprRef& newOperand) final {
        auto& sequence = lib::get<ExprRef<SequenceView>>(newOperand);
        shiftStartingIndicesUp(index, sequence->view()
                                          .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                                          .numberElements());
        auto& members = sequence->view()
                            .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                            .getMembers<SequenceInnerType>();
        for (size_t i = 0; i < members.size(); i++) {
            op->addMemberAndNotify(op->startingIndices[index] + i, members[i]);
        }
        op->innerSequenceTriggers.insert(
            op->innerSequenceTriggers.begin() + index,
            make_shared<
                OpFlattenOneLevel<SequenceInnerType>::InnerSequenceTrigger>(
                op, index));
        sequence->addTrigger(op->innerSequenceTriggers[index]);
        correctInnerSequenceTriggerIndices(index + 1);
    }

    void subsequenceChanged(UInt, UInt) final {
        // ignore
    }
    void memberReplaced(UInt index, const AnyExprRef& oldMember) final {
        valueRemoved(index, oldMember);
        valueAdded(index, op->operand->view()
                              .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                              .template getMembers<SequenceInnerType>()[index]);
    }
    void memberHasBecomeDefined(UInt index) { todoImpl(index); }
    void memberHasBecomeUndefined(UInt index) { todoImpl(index); }
    void fixIndicesForSwap(UInt index1, UInt index2, Int& index1SequenceSize,
                           Int& index2SequenceSize) {
        UInt startingIndexAfterIndex2 =
            (index2 + 1 < op->startingIndices.size())
                ? op->startingIndices[index2 + 1]
                : op->numberElements();
        index2SequenceSize =
            startingIndexAfterIndex2 - op->startingIndices[index2];
        index1SequenceSize =
            op->startingIndices[index1 + 1] - op->startingIndices[index1];
        if (index1SequenceSize == index2SequenceSize) {
            return;
        }
        Int diff = index2SequenceSize - index1SequenceSize;
        for (size_t i = index1 + 1; i <= index2; i++) {
            op->startingIndices[i] += diff;
        }
        return;
    }
    void positionsSwapped(UInt index1, UInt index2) {
        if (index1 == index2) {
            return;
        }
        std::swap(op->innerSequenceTriggers[index1],
                  op->innerSequenceTriggers[index2]);
        op->innerSequenceTriggers[index1]->index = index1;
        op->innerSequenceTriggers[index2]->index = index2;
        if (index1 > index2) {
            swap(index1, index2);
        }
        UInt startingIndex1BeforeSwap = op->startingIndices[index1];
        UInt startingIndex2BeforeSwap = op->startingIndices[index2];
        Int index1SequenceSizeBeforeSwap, index2SequenceSizeBeforeSwap;
        fixIndicesForSwap(index1, index2, index1SequenceSizeBeforeSwap,
                          index2SequenceSizeBeforeSwap);
        debug_code(op->assertValidStartingIndices());
        // now that startingIndices have been updated, move the actual sequence
        // members. start by swapping the positions of first n elements of
        // sequence 1 (position in the flattened output) with the first n
        // elements of sequence 2. N here is the size of the smaller sequence.
        swapElements(
            startingIndex1BeforeSwap, startingIndex2BeforeSwap,
            min(index1SequenceSizeBeforeSwap, index2SequenceSizeBeforeSwap));

        if (index1SequenceSizeBeforeSwap == index2SequenceSizeBeforeSwap) {
            // nothing left to do if the sequences were the same size
            return;
        } else if (startingIndex1BeforeSwap + index1SequenceSizeBeforeSwap ==
                       startingIndex2BeforeSwap &&
                   (index1SequenceSizeBeforeSwap == 0 ||
                    index2SequenceSizeBeforeSwap == 0)) {
            // no shifting required
            return;
        }
        // if the sequences were different sizes, take the remaining elements
        // (those from the longer sequence) and move them to be in their new
        // position.
        UInt fromStart, toStart, moveLength;

        if (index1SequenceSizeBeforeSwap < index2SequenceSizeBeforeSwap) {
            fromStart = startingIndex2BeforeSwap + index1SequenceSizeBeforeSwap;
            toStart = startingIndex1BeforeSwap + index1SequenceSizeBeforeSwap;
            moveLength =
                index2SequenceSizeBeforeSwap - index1SequenceSizeBeforeSwap;
        } else {
            fromStart = startingIndex1BeforeSwap + index2SequenceSizeBeforeSwap;
            toStart = startingIndex2BeforeSwap + index2SequenceSizeBeforeSwap;
            moveLength =
                index1SequenceSizeBeforeSwap - index2SequenceSizeBeforeSwap;
        }
        moveElements(fromStart, toStart, moveLength);
    }
    void moveElements(UInt start1, UInt start2, UInt length) {
        debug_log("start1=" << start1 << ", start2=" << start2
                            << ", length=" << length
                            << ", numberElements=" << op->numberElements());
        vector<ExprRef<SequenceInnerType>> elements;
        for (size_t i = start1 + length; i > start1; i--) {
            elements.emplace_back(
                op->removeMemberAndNotify<SequenceInnerType>(i - 1));
        }
        if (start2 >= start1) {
            start2 -= length;
        }
        UInt i = 0;
        while (!elements.empty()) {
            op->addMemberAndNotify((start2) + i, elements.back());
            ++i;
            elements.pop_back();
        }
    }

    void swapElements(UInt start1, UInt start2, UInt length) {
        for (UInt i = 0; i < length; i++) {
            op->swapAndNotify<SequenceInnerType>(start1 + i, start2 + i);
        }
    }
};

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::updateVarViolationsImpl(
    const ViolationContext&, ViolationContainer&) {}

template <typename SequenceInnerType>
std::ostream& OpFlattenOneLevel<SequenceInnerType>::dumpState(
    std::ostream& os) const {
    os << getOpName() << "(value=";
    prettyPrint(os, this->view().checkedGet(OP_FLATTEN_DEFAULT_ERROR));
    os << "\nstartingIndices: " << startingIndices;
    debug_code(assertValidStartingIndices());
    os << "\noperand: ";
    operand->dumpState(os);
    os << ")";
    return os;
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::evaluateImpl() {
    members.emplace<ExprRefVec<SequenceInnerType>>();
    operand->evaluate();
    this->setAppearsDefined(operand->getViewIfDefined().hasValue());
    if (this->appearsDefined()) {
        reevaluate();
    }
}
template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::reattachAllInnerSequenceTriggers(
    bool deleteFirst) {
    if (deleteFirst) {
        for (auto& trigger : innerSequenceTriggers) {
            deleteTrigger(trigger);
        }
        innerSequenceTriggers.clear();
    }
    auto& innerSequences = operand->view()
                               .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                               .getMembers<SequenceView>();
    for (UInt i = 0; i < innerSequences.size(); i++) {
        innerSequenceTriggers.emplace_back(
            make_shared<InnerSequenceTrigger>(this, i));
        innerSequences[i]->addTrigger(innerSequenceTriggers.back());
    }
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::startTriggeringImpl() {
    if (!operandTrigger) {
        operandTrigger = std::make_shared<OperandTrigger>(this);
        operand->addTrigger(operandTrigger);
        reattachAllInnerSequenceTriggers(false);
        operand->startTriggering();
        for (auto& innerSequence : operand->view()
                                       .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                                       .getMembers<SequenceView>()) {
            innerSequence->startTriggering();
        }
    }
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::stopTriggeringOnChildren() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
        for (auto& trigger : innerSequenceTriggers) {
            deleteTrigger(trigger);
        }
        innerSequenceTriggers.clear();
    }
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::stopTriggering() {
    if (operandTrigger) {
        stopTriggeringOnChildren();
        operand->stopTriggering();
        for (auto& innerSequence : operand->view()
                                       .checkedGet(OP_FLATTEN_DEFAULT_ERROR)
                                       .getMembers<SequenceView>()) {
            innerSequence->stopTriggering();
        }
    }
}

template <typename SequenceInnerType>
ExprRef<SequenceView>
OpFlattenOneLevel<SequenceInnerType>::deepCopyForUnrollImpl(
    const ExprRef<SequenceView>&, const AnyIterRef& iterator) const {
    auto newOp = make_shared<OpFlattenOneLevel<SequenceInnerType>>(
        operand->deepCopyForUnroll(operand, iterator));
    return newOp;
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func, PathExtension path) {
    this->operand = findAndReplace(operand, func, path);
}

template <typename SequenceInnerType>
std::pair<bool, ExprRef<SequenceView>>
OpFlattenOneLevel<SequenceInnerType>::optimiseImpl(ExprRef<SequenceView>&,
                                                   PathExtension path) {
    auto newOp = make_shared<OpFlattenOneLevel>(operand);
    bool optimised = false;
    optimised |= optimise(ExprRef<SequenceView>(newOp), newOp->operand, path);
    return make_pair(optimised, newOp);
}
template <typename SequenceInnerType>
string OpFlattenOneLevel<SequenceInnerType>::getOpName() const {
    return toString(
        "OpFlattenOneLevel<",
        TypeAsString<
            typename AssociatedValueType<SequenceInnerType>::type>::value,
        ">");
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    sanityCheck(operand->appearsDefined() == this->appearsDefined(),
                toString("operand defined = ", operand->appearsDefined(),
                         " operator defined = ", this->appearsDefined()));
    auto view = operand->view();
    if (!view) {
        return;
    }
    auto& operandView = *view;
    auto& innerSequences = operandView.getMembers<SequenceView>();
    UInt total = 0;
    UInt totalNumberUndefined = 0;
    sanityCheck(
        innerSequences.size() == startingIndices.size(),
        toString("startingIndices has size ", startingIndices.size(),
                 " number of inner sequences is ", innerSequences.size()));

    for (size_t i = 0; i < innerSequences.size(); i++) {
        sanityCheck(startingIndices[i] == total,
                    toString("Error: expected starting index at position ", i,
                             " to be ", total));
        auto& innerView =
            innerSequences[i]->view().checkedGet(OP_FLATTEN_DEFAULT_ERROR);
        totalNumberUndefined += innerView.numberUndefined;
        auto innerMembers = innerView.getMembers<SequenceInnerType>();
        for (size_t j = 0; j < innerMembers.size(); j++) {
            auto* innerMemberPtr = &(*innerMembers[j]);
            sanityCheck(total + j < numberElements(),
                        "Not enough members in OpFlattenOneLevel");
            auto* memberPtr = &(*(getMembers<SequenceInnerType>()[total + j]));
            sanityCheck(innerMemberPtr == memberPtr,
                        "member is not at the right position.");
        }
        total += innerView.numberElements();
    }
    sanityEqualsCheck(totalNumberUndefined, numberUndefined);
}

template <typename Op>
struct OpMaker;

template <typename SequenceInnerType>
struct OpMaker<OpFlattenOneLevel<SequenceInnerType>> {
    static ExprRef<SequenceView> make(ExprRef<SequenceView> o);
};

template <typename SequenceInnerType>
ExprRef<SequenceView> OpMaker<OpFlattenOneLevel<SequenceInnerType>>::make(
    ExprRef<SequenceView> o) {
    return make_shared<OpFlattenOneLevel<SequenceInnerType>>(move(o));
}

#define opFlattenOneLevelInstantiators(name)       \
    template struct OpFlattenOneLevel<name##View>; \
    template struct OpMaker<OpFlattenOneLevel<name##View>>;

buildForAllTypes(opFlattenOneLevelInstantiators, );
#undef opFlattenOneLevelInstantiators
