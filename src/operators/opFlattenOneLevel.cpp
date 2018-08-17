#include "operators/opFlattenOneLevel.h"
#include <iostream>
#include <memory>
#include "types/sequence.h"
#include "utils/ignoreUnused.h"

using namespace std;
template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::assertValidStartingIndices() {
    auto& innerSequences = operand->view().getMembers<SequenceView>();
    UInt total = 0;
    bool success = true;
    if (innerSequences.size() != startingIndices.size()) {
        success = false;
        cerr << "Error: startingIndices not same length as number of inner "
                "sequences.\n";
    }
    if (success) {
        for (size_t i = 0; i < innerSequences.size(); i++) {
            if (startingIndices[i] != total) {
                success = false;
                cerr << "Error: expected starting index at position " << i
                     << " to be " << total;
                break;
            }
            total += innerSequences[i]->view().numberElements();
        }
    }
    if (!success) {
        cerr << "\nOperand sequences:\n";
        for (auto& innerSequence : innerSequences) {
            prettyPrint(cerr, innerSequence->view()) << endl;
        }
        assert(false);
        abort();
    }
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::reevaluate() {
    auto& operandMembers =
        mpark::get<ExprRefVec<SequenceView>>(operand->view().members);
    startingIndices.resize(operandMembers.size());
    silentClear();
    for (size_t i = 0; i < operandMembers.size(); i++) {
        auto& innerSequenceMembers = mpark::get<ExprRefVec<SequenceInnerType>>(
            operandMembers[i]->view().members);
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
        op->removeMemberAndNotify<SequenceInnerType>(index + indexOfRemoved);
        for (size_t i = index + 1; i < op->startingIndices.size(); i++) {
            --op->startingIndices[i];
        }
    }
    inline void valueAdded(UInt indexOfAdded, const AnyExprRef& member) final {
        op->addMemberAndNotify(index + indexOfAdded,
                               mpark::get<ExprRef<SequenceInnerType>>(member));
        for (size_t i = index + 1; i < op->startingIndices.size(); i++) {
            ++op->startingIndices[i];
        }
    }

    inline void possibleSubsequenceChange(UInt startIndex,
                                          UInt endIndex) final {
        op->notifyPossibleSubsequenceChange<SequenceInnerType>(
            index + startIndex, index + endIndex);
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        op->changeSubsequenceAndNotify<SequenceInnerType>(index + startIndex,
                                                          index + endIndex);
    }
    inline void beginSwaps() final {}
    inline void positionsSwapped(UInt index1, UInt index2) final {
        op->swapAndNotify<SequenceInnerType>(index + index1, index + index2);
    }
    inline void endSwaps() final {}
    inline void memberHasBecomeDefined(UInt) {}
    inline void memberHasBecomeUndefined(UInt) {}
    void hasBecomeUndefined() {}
    void hasBecomeDefined() {}
    void reattachTrigger() {
        deleteTrigger(op->innerSequenceTriggers[index]);
        auto trigger = make_shared<
            OpFlattenOneLevel<SequenceInnerType>::InnerSequenceTrigger>(op,
                                                                        index);
        op->operand->view()
            .template getMembers<SequenceView>()[index]
            ->addTrigger(trigger);
        op->innerSequenceTriggers[index] = trigger;
    }
    void valueChanged() { todoImpl(); }
    void possibleValueChange() { op->notifyPossibleSequenceValueChange(); }
};

template <typename SequenceInnerType>
struct OpFlattenOneLevel<SequenceInnerType>::OperandTrigger
    : public SequenceTrigger {
    OpFlattenOneLevel<SequenceInnerType>* op;
    OperandTrigger(OpFlattenOneLevel<SequenceInnerType>* op) : op(op) {}
    void possibleValueChange() final {}
    void valueChanged() {
        op->notifyPossibleSequenceValueChange();

        op->reevaluate();
        op->reattachAllInnerSequenceTriggers(true);
        op->notifyEntireSequenceChange();
    }

    void hasBecomeUndefined() final {
        op->defined = false;
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers,
                      true);
    }
    void hasBecomeDefined() final {
        op->defined = true;
        op->reevaluate();
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers,
                      true);
    }

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
            op->startingIndices[i] -= diff;
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
        op->startingIndices.emplace_back();
        for (size_t i = op->startingIndices.size(); i > index + 1; i--) {
            op->startingIndices[index - 1] = op->startingIndices[i - 2];
            op->startingIndices[i - 1] += shiftAmount;
        }
    }
    void valueAdded(UInt index, const AnyExprRef& newOperand) final {
        auto& sequence = mpark::get<ExprRef<SequenceView>>(newOperand);
        shiftStartingIndicesUp(index, sequence->view().numberElements());
        auto& members = sequence->view().getMembers<SequenceInnerType>();
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

    void possibleSubsequenceChange(UInt, UInt) final {
        // ignore
    }
    void subsequenceChanged(UInt, UInt) final {
        // ignore
    }
    void memberHasBecomeDefined(UInt index) {
        op->notifyMemberDefined<SequenceInnerType>(index);
    }
    void memberHasBecomeUndefined(UInt index) {
        op->notifyMemberUndefined<SequenceInnerType>(index);
    }
    void beginSwaps() final {}
    void endSwaps() final {}
    void positionsSwapped(UInt index1, UInt index2) {
        todoImpl(index1, index2);
    }
};

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::updateVarViolations(
    const ViolationContext&, ViolationContainer&) {}

template <typename SequenceInnerType>
std::ostream& OpFlattenOneLevel<SequenceInnerType>::dumpState(
    std::ostream& os) const {
    os << "OpFlattenOneLevel<SequenceInnerType>: value=";
    prettyPrint(os, this->view());
    os << "\noperand: ";
    operand->dumpState(os);
    os << ")";
    return os;
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::evaluateImpl() {
    operand->evaluate();
    defined = !operand->isUndefined();
    if (defined) {
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
    auto& innerSequences = operand->view().getMembers<SequenceView>();
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
        for (auto& innerSequence : operand->view().getMembers<SequenceView>()) {
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
        for (auto& innerSequence : operand->view().getMembers<SequenceView>()) {
            innerSequence->stopTriggering();
        }
    }
}

template <typename SequenceInnerType>
ExprRef<SequenceView>
OpFlattenOneLevel<SequenceInnerType>::deepCopySelfForUnrollImpl(
    const ExprRef<SequenceView>&, const AnyIterRef& iterator) const {
    auto newOp = make_shared<OpFlattenOneLevel<SequenceView>>(
        operand->deepCopySelfForUnroll(operand, iterator));
    newOp->defined = defined;
    newOp->startingIndices = startingIndices;
    return newOp;
}

template <typename SequenceInnerType>
void OpFlattenOneLevel<SequenceInnerType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->operand = findAndReplace(operand, func);
}

template <typename SequenceInnerType>
bool OpFlattenOneLevel<SequenceInnerType>::isUndefined() {
    return !defined || this->numberUndefined > 0;
}
template <typename SequenceInnerType>
std::pair<bool, ExprRef<SequenceView>>
OpFlattenOneLevel<SequenceInnerType>::optimise(PathExtension path) {
    return make_pair(operand->optimise(path.extend(operand)).first,
                     mpark::get<ExprRef<SequenceView>>(path.expr));
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
