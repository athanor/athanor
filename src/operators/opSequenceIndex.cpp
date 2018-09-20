#include "operators/opSequenceIndex.h"
#include <iostream>
#include <memory>
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::addTriggerImpl(
    const shared_ptr<SequenceMemberTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    triggers.emplace_back(getTriggerBase(trigger));
    if (locallyDefined) {
        getMember().get()->addTrigger(trigger, includeMembers, memberIndex);
    }
}

template <typename SequenceMemberViewType>
OptionalRef<ExprRef<SequenceMemberViewType>>
OpSequenceIndex<SequenceMemberViewType>::getMember() {
    debug_code(assert(this->appearsDefined()));
    debug_code(assert(indexOperand->appearsDefined()));
    debug_code(assert(cachedIndex >= 0));
    auto view = sequenceOperand->getViewIfDefined();
    if (!view || cachedIndex >= (Int)(*view).numberElements()) {
        return EmptyOptional();
    }

    auto& member = (*view).getMembers<SequenceMemberViewType>()[cachedIndex];
    return member;
}

template <typename SequenceMemberViewType>
OptionalRef<const ExprRef<SequenceMemberViewType>>
OpSequenceIndex<SequenceMemberViewType>::getMember() const {
    debug_code(assert(this->appearsDefined()));
    debug_code(assert(indexOperand->appearsDefined()));
    debug_code(assert(cachedIndex >= 0));
    auto view = sequenceOperand->getViewIfDefined();
    if (!view || cachedIndex >= (Int)(*view).numberElements()) {
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
        return EmptyOptional();
    }
}
template <typename SequenceMemberViewType>
OptionalRef<const SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::view() const {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return EmptyOptional();
    }
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::reevaluate(
    bool recalculateCachedIndex) {
    if (!indexOperand->appearsDefined()) {
        locallyDefined = false;
        this->setAppearsDefined(false);
        return;
    }
    if (recalculateCachedIndex) {
        auto indexView = indexOperand->getViewIfDefined();
        if (!indexView) {
            locallyDefined = false;
        } else {
            locallyDefined = true;
            cachedIndex = (*indexView).value;
        }
    }
    setAppearsDefined(locallyDefined && getMember().get()->appearsDefined());
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::evaluateImpl() {
    sequenceOperand->evaluate();
    indexOperand->evaluate();
    reevaluate();
}

template <typename SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::OpSequenceIndex(
    OpSequenceIndex<SequenceMemberViewType>&& other)
    : ExprInterface<SequenceMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      sequenceOperand(move(other.sequenceOperand)),
      indexOperand(move(other.indexOperand)),
      cachedIndex(move(other.cachedIndex)),
      locallyDefined(move(other.locallyDefined)),
      sequenceOperandTrigger(move(other.sequenceOperandTrigger)),
      sequenceMemberTrigger(move(other.sequenceMemberTrigger)),
      indexTrigger(move(other.indexTrigger)) {
    setTriggerParent(this, sequenceOperandTrigger, sequenceMemberTrigger,
                     indexTrigger);
}

template <typename SequenceMemberViewType>
bool OpSequenceIndex<SequenceMemberViewType>::eventForwardedAsDefinednessChange(
    bool recalculateIndex) {
    bool wasDefined = this->appearsDefined();
    bool wasLocallyDefined = locallyDefined;
    reevaluate(recalculateIndex);
    if (!wasLocallyDefined && locallyDefined) {
        reattachSequenceMemberTrigger();
    }
    if (wasDefined && !this->appearsDefined()) {
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, triggers,
                      true);
        return true;
    } else if (!wasDefined && this->appearsDefined()) {
        visitTriggers(
            [&](auto t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            triggers, true);
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
        if (!op->eventForwardedAsDefinednessChange(false) &&
            (Int)index <= op->cachedIndex) {
            visitTriggers(
                [&](auto t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
        }
    }

    inline void valueRemoved(UInt index, const AnyExprRef&) {
        if (!op->locallyDefined) {
            return;
        }
        if (!op->eventForwardedAsDefinednessChange(false) &&
            (Int)index <= op->cachedIndex) {
            visitTriggers(
                [&](auto& t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
        }
    }

    void subsequenceChanged(UInt, UInt) {
        // ignore, already triggering on member
    }

    void memberHasBecomeUndefined(UInt index) final {
        if (!op->appearsDefined() || (Int)index != op->cachedIndex) {
            return;
        }
        op->setAppearsDefined(false);
    }

    void memberHasBecomeDefined(UInt index) final {
        if (!op->locallyDefined || (Int)index != op->cachedIndex) {
            return;
        }
        op->setAppearsDefined(true);
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
        if (!op->eventForwardedAsDefinednessChange(false)) {
            visitTriggers(
                [&](auto t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
            op->reattachSequenceMemberTrigger();
        }
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
        if (!op->appearsDefined()) {
            return;
        }
        op->setAppearsDefined(false);
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers,
                      true);
    }

    void hasBecomeDefined() {
        op->reevaluate(true);
        if (!op->appearsDefined()) {
            return;
        }
        op->reattachSequenceMemberTrigger();
        visitTriggers(
            [&](auto t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            op->triggers, true);
    }
    void reattachTrigger() final {
        deleteTrigger(op->sequenceOperandTrigger);
        if (op->sequenceMemberTrigger) {
            deleteTrigger(op->sequenceMemberTrigger);
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
        if (!op->eventForwardedAsDefinednessChange(true)) {
            visitTriggers(
                [&](auto t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
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
    void hasBecomeDefined() {
        op->reevaluate();
        if (op->appearsDefined()) {
            return;
        }
        visitTriggers(
            [&](auto t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            op->triggers, true);
    }

    void hasBecomeUndefined() {
        if (!op->appearsDefined()) {
            return;
        }
        op->locallyDefined = false;
        op->setAppearsDefined(false);
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers,
                      true);
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
        reattachSequenceMemberTrigger();
        sequenceOperand->startTriggering();
    }
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::reattachSequenceMemberTrigger() {
    if (sequenceMemberTrigger) {
        deleteTrigger(sequenceMemberTrigger);
    }
    sequenceMemberTrigger = make_shared<SequenceOperandTrigger>(this);
    sequenceOperand->addTrigger(sequenceMemberTrigger, true, cachedIndex);
}

template <typename SequenceMemberViewType>
void OpSequenceIndex<SequenceMemberViewType>::stopTriggeringOnChildren() {
    if (indexTrigger) {
        deleteTrigger(indexTrigger);
        deleteTrigger(sequenceOperandTrigger);
        deleteTrigger(sequenceMemberTrigger);
        indexTrigger = nullptr;
        sequenceOperandTrigger = nullptr;
        sequenceMemberTrigger = nullptr;
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
    indexOperand->updateVarViolationsImpl(vioContext, vioContainer);
    auto sequenceMember = getMember();
    if (locallyDefined && sequenceMember) {
        (*sequenceMember)->updateVarViolations(vioContext, vioContainer);
    } else {
        sequenceOperand->updateVarViolations(vioContext, vioContainer);
    }
}

template <typename SequenceMemberViewType>
ExprRef<SequenceMemberViewType>
OpSequenceIndex<SequenceMemberViewType>::deepCopySelfForUnrollImpl(
    const ExprRef<SequenceMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpSequenceIndex =
        make_shared<OpSequenceIndex<SequenceMemberViewType>>(
            sequenceOperand->deepCopySelfForUnroll(sequenceOperand, iterator),
            indexOperand->deepCopySelfForUnrollImpl(indexOperand, iterator));
    newOpSequenceIndex->cachedIndex = cachedIndex;
    newOpSequenceIndex->locallyDefined = locallyDefined;
    return newOpSequenceIndex;
}

template <typename SequenceMemberViewType>
std::ostream& OpSequenceIndex<SequenceMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opSequenceIndex(value=";
    prettyPrint(os, this->getViewIfDefined());
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
OpSequenceIndex<SequenceMemberViewType>::optimise(PathExtension path) {
    bool changeMade = false;
    auto optResult = sequenceOperand->optimise(path.extend(sequenceOperand));
    changeMade |= optResult.first;
    sequenceOperand = optResult.second;
    auto optResult2 = indexOperand->optimise(path.extend(indexOperand));
    changeMade |= optResult2.first;
    indexOperand = optResult2.second;
    return make_pair(changeMade,
                     mpark::get<ExprRef<SequenceMemberViewType>>(path.expr));
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
