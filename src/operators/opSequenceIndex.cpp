#include "operators/opSequenceIndex.h"
#include <iostream>
#include <memory>
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"

using namespace std;

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::addTrigger(
    const shared_ptr<SequenceMemberTriggerType>& trigger) {
    sequenceOperand->view()
        .getMembers<SequenceMemberViewType>()[cachedIndex]
        ->addTrigger(trigger);
    triggers.emplace_back(static_pointer_cast<TriggerBase>(trigger));
}

template <typename SequenceMemberViewType>
SequenceMemberViewType& OpSequenceIndex<SequenceMemberViewType>::view() {
    return sequenceOperand->view()
        .getMembers<SequenceMemberViewType>()[cachedIndex]
        ->view();
}
template <typename SequenceMemberViewType>
const SequenceMemberViewType& OpSequenceIndex<SequenceMemberViewType>::view()
    const {
    return sequenceOperand->view()
        .getMembers<SequenceMemberViewType>()[cachedIndex]
        ->view();
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::evaluate() {
    indexOperand->evaluate();
    sequenceOperand->evaluate();
    cachedIndex = indexOperand->view().value - 1;
}

template <typename SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::OpSequenceIndex(
    OpSequenceIndex<SequenceMemberViewType>&& other)
    : ExprInterface<SequenceMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      sequenceOperand(move(other.sequenceOperand)),
      indexOperand(move(other.indexOperand)),
      cachedIndex(move(other.cachedIndex)),
      sequenceTrigger(move(other.sequenceTrigger)),
      indexTrigger(move(other.indexTrigger)) {
    setTriggerParent(this, indexTrigger, sequenceTrigger);
}

template <typename SequenceMemberViewType>
struct OpSequenceIndex<SequenceMemberViewType>::SequenceOperandTrigger
    : public SequenceTrigger {
    OpSequenceIndex* op;
    SequenceOperandTrigger(OpSequenceIndex* op) : op(op) {}
    void possibleValueChange() final {
        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
    }
    void valueChanged() final {
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            op->triggers);
    }
    inline void valueRemoved(UInt index, const AnyExprRef&) {
        if (index > op->cachedIndex) {
            return;
        }
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            op->triggers);
    }
    inline void valueAdded(UInt index, const AnyExprRef&) final {
        if (index > op->cachedIndex) {
            return;
        }
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            op->triggers);
    }
    inline void possibleSubsequenceChange(UInt, UInt) final {}

    inline void subsequenceChanged(UInt, UInt) final {}
    inline void beginSwaps() final {}
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt index1, UInt index2) final {
        if ((index1 != op->cachedIndex && index2 != op->cachedIndex) ||
            index1 == index2) {
            return;
        }
        if (index1 != op->cachedIndex) {
            swap(index1, index2);
        }
        op->cachedIndex = index2;
        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
        op->cachedIndex = index1;
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            op->triggers);
    }
    void reattachTrigger() final {
        deleteTrigger(op->sequenceTrigger);
        auto trigger = make_shared<
            OpSequenceIndex<SequenceMemberViewType>::SequenceOperandTrigger>(
            op);
        op->sequenceOperand->addTrigger(trigger);
        op->sequenceTrigger = trigger;
    }
};

template <typename SequenceMemberViewType>
struct OpSequenceIndex<SequenceMemberViewType>::IndexTrigger
    : public IntTrigger {
    OpSequenceIndex* op;
    IndexTrigger(OpSequenceIndex* op) : op(op) {}
    void possibleValueChange() final {}
    void valueChanged() final {
        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
        op->cachedIndex = op->indexOperand->view().value - 1;
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            op->triggers);
    }

    void reattachTrigger() final {
        deleteTrigger(op->indexTrigger);
        auto trigger =
            make_shared<OpSequenceIndex<SequenceMemberViewType>::IndexTrigger>(
                op);
        op->indexOperand->addTrigger(trigger);
        op->indexTrigger = trigger;
    }
};

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::startTriggering() {
    if (!indexTrigger) {
        indexTrigger =
            make_shared<OpSequenceIndex<SequenceMemberViewType>::IndexTrigger>(
                this);
        sequenceTrigger = make_shared<
            OpSequenceIndex<SequenceMemberViewType>::SequenceOperandTrigger>(
            this);
        indexOperand->addTrigger(indexTrigger);
        sequenceOperand->addTrigger(sequenceTrigger);
        indexOperand->startTriggering();
        sequenceOperand->startTriggering();
    }
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::stopTriggeringOnChildren() {
    if (indexTrigger) {
        deleteTrigger(indexTrigger);
        deleteTrigger(sequenceTrigger);
        indexTrigger = nullptr;
        sequenceTrigger = nullptr;
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
void OpSequenceIndex<SequenceMemberViewType>::updateViolationDescription(
    UInt parentViolation, ViolationDescription& vioDesc) {
    sequenceOperand->view()
        .getMembers<SequenceMemberViewType>()[cachedIndex]
        ->updateViolationDescription(parentViolation, vioDesc);
}

template <typename SequenceMemberViewType>
ExprRef<SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::deepCopySelfForUnroll(
    const ExprRef<SequenceMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpSequenceIndex =
        make_shared<OpSequenceIndex<SequenceMemberViewType>>(
            sequenceOperand->deepCopySelfForUnroll(sequenceOperand, iterator),
            indexOperand->deepCopySelfForUnroll(indexOperand, iterator));
    newOpSequenceIndex->cachedIndex = cachedIndex;
    return newOpSequenceIndex;
}

template <typename SequenceMemberViewType>
std::ostream& OpSequenceIndex<SequenceMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opSequenceIndex(sequence=";
    sequenceOperand->dumpState(os) << ",\n";
    os << "index=";
    indexOperand->dumpState(os) << ")";
    return os;
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->indexOperand = findAndReplace(indexOperand, func);
    this->sequenceOperand = findAndReplace(sequenceOperand, func);
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
