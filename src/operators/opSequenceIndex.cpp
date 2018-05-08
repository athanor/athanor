#include "operators/opSequenceIndex.h"
#include <iostream>
#include <memory>
#include "utils/ignoreUnused.h"
using namespace std;

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::addTrigger(
    const shared_ptr<SequenceMemberTriggerType>& trigger) {
    triggers.emplace_back(getTriggerBase(trigger));
}

template <typename SequenceMemberViewType>
ExprRef<SequenceMemberViewType>&
OpSequenceIndex<SequenceMemberViewType>::getMember() {
    debug_code(
        assert(!indexOperand->isUndefined() && cachedIndex >= 0 &&
               !sequenceOperand->isUndefined() &&
               cachedIndex < (Int)sequenceOperand->view().numberElements()));
    auto& member = sequenceOperand->view()
                       .getMembers<SequenceMemberViewType>()[cachedIndex];
    debug_code(assert(!member->isUndefined()));
    return member;
}

template <typename SequenceMemberViewType>
const ExprRef<SequenceMemberViewType>&
OpSequenceIndex<SequenceMemberViewType>::getMember() const {
    debug_code(
        assert(!indexOperand->isUndefined() && cachedIndex >= 0 &&
               !sequenceOperand->isUndefined() &&
               cachedIndex < (Int)sequenceOperand->view().numberElements()));

    const auto& member = sequenceOperand->view()
                             .getMembers<SequenceMemberViewType>()[cachedIndex];
    debug_code(assert(!member->isUndefined()));
    return member;
}

template <typename SequenceMemberViewType>
SequenceMemberViewType& OpSequenceIndex<SequenceMemberViewType>::view() {
    return getMember()->view();
}
template <typename SequenceMemberViewType>
const SequenceMemberViewType& OpSequenceIndex<SequenceMemberViewType>::view()
    const {
    return getMember()->view();
}
template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::reevaluateDefined() {
    defined = !indexOperand->isUndefined() && cachedIndex >= 0 &&
              !sequenceOperand->isUndefined() &&
              cachedIndex < (Int)sequenceOperand->view().numberElements() &&
              !getMember()->isUndefined();
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::evaluate() {
    sequenceOperand->evaluate();
    indexOperand->evaluate();
    if (indexOperand->isUndefined()) {
        defined = false;
    } else {
        cachedIndex = indexOperand->view().value - 1;
        reevaluateDefined();
    }
}

template <typename SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::OpSequenceIndex(
    OpSequenceIndex<SequenceMemberViewType>&& other)
    : ExprInterface<SequenceMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      sequenceOperand(move(other.sequenceOperand)),
      indexOperand(move(other.indexOperand)),
      cachedIndex(move(other.cachedIndex)),
      defined(move(other.defined)),
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
        if (!op->defined) {
            return;
        }

        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
    }
    void valueChanged() final {
        bool wasDefined = op->defined;
        op->reevaluateDefined();
        if (wasDefined && !op->defined) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        } else if (!wasDefined && op->defined) {
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                          op->triggers);
        } else if (op->defined) {
            visitTriggers([&](auto& t) { t->valueChanged(); }, op->triggers);
        }
    }

    inline void valueRemoved(UInt index, const AnyExprRef&) {
        if (!op->defined) {
            return;
        }
        op->defined =
            op->cachedIndex < (Int)op->sequenceOperand->view().numberElements();
        if (!op->defined) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
            return;
        }

        if ((Int)index > op->cachedIndex) {
            return;
        }
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();

            },
            op->triggers);
    }

    inline void valueAdded(UInt index, const AnyExprRef&) final {
        if (!op->defined) {
            op->reevaluateDefined();
            if (op->defined) {
                visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                              op->triggers);
            }
            return;
        }
        if ((Int)index > op->cachedIndex) {
            return;
        }
        visitTriggers([&](auto& t) { t->valueChanged(); }, op->triggers);
    }

    inline void possibleSubsequenceChange(UInt startIndex,
                                          UInt endIndex) final {
        if (op->defined && (Int)startIndex <= op->cachedIndex &&
            (Int)endIndex > op->cachedIndex) {
            visitTriggers([&](auto& t) { t->possibleValueChange(); },
                          op->triggers);
        }
    }

    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        if (op->defined && (Int)startIndex <= op->cachedIndex &&
            (Int)endIndex > op->cachedIndex) {
            visitTriggers([&](auto& t) { t->valueChanged(); }, op->triggers);
        }
    }

    inline void beginSwaps() final {}
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt index1, UInt index2) final {
        if (!op->defined) {
            return;
        }

        if (((Int)index1 != op->cachedIndex &&
             (Int)index2 != op->cachedIndex) ||
            index1 == index2) {
            return;
        }
        if ((Int)index1 != op->cachedIndex) {
            swap(index1, index2);
        }
        op->cachedIndex = index2;
        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
        op->cachedIndex = index1;
        visitTriggers([&](auto& t) { t->valueChanged(); }, op->triggers);
    }

    void reattachTrigger() final {
        deleteTrigger(op->sequenceTrigger);
        auto trigger = make_shared<
            OpSequenceIndex<SequenceMemberViewType>::SequenceOperandTrigger>(
            op);
        op->sequenceOperand->addTrigger(trigger);
        op->sequenceTrigger = trigger;
    }

    inline void hasBecomeUndefined() final {
        if (!op->defined) {
            return;
        }
        op->defined = false;
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers);
    }
    void hasBecomeDefined() final {
        op->reevaluateDefined();
        if (!op->defined) {
            return;
        }
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers);
    }
    void memberHasBecomeUndefined(UInt index) final {
        if (!op->defined) {
            return;
        }
        if ((Int)index != op->cachedIndex) {
            return;
        }
        op->defined = false;
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers);
    }
    void memberHasBecomeDefined(UInt) final {
        op->reevaluateDefined();
        if (!op->defined) {
            return;
        }
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers);
    }
};

template <typename SequenceMemberViewType>
struct OpSequenceIndex<SequenceMemberViewType>::IndexTrigger
    : public IntTrigger {
    OpSequenceIndex* op;
    IndexTrigger(OpSequenceIndex* op) : op(op) {}
    void possibleValueChange() final {
        if (!op->defined) {
            return;
        }

        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
    }

    void valueChanged() final {
        op->cachedIndex = op->indexOperand->view().value - 1;
        bool wasDefined = op->defined;
        op->reevaluateDefined();
        if (wasDefined && !op->defined) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        } else if (!wasDefined && op->defined) {
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                          op->triggers);
        } else if (op->defined) {
            visitTriggers([&](auto& t) { t->valueChanged(); }, op->triggers);
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
    inline void hasBecomeUndefined() final {
        if (!op->defined) {
            return;
        }
        op->defined = false;
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers);
    }
    void hasBecomeDefined() final {
        op->cachedIndex = op->indexOperand->view().value - 1;
        op->reevaluateDefined();
        if (!op->defined) {
            return;
        }
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers);
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
    indexOperand->updateViolationDescription(parentViolation, vioDesc);
    if (defined) {
        getMember()->updateViolationDescription(parentViolation, vioDesc);
    } else {
        sequenceOperand->updateViolationDescription(parentViolation, vioDesc);
    }
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
    newOpSequenceIndex->defined = defined;
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
    this->sequenceOperand = findAndReplace(sequenceOperand, func);
    this->indexOperand = findAndReplace(indexOperand, func);
}

template <typename SequenceMemberViewType>
bool OpSequenceIndex<SequenceMemberViewType>::isUndefined() {
    return !defined;
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
