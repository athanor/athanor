#include "operators/opTupleIndex.h"
#include <iostream>
#include <memory>
#include "utils/ignoreUnused.h"
using namespace std;

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::addTrigger(
    const shared_ptr<TupleMemberTriggerType>& trigger) {
    triggers.emplace_back(getTriggerBase(trigger));
}

template <typename TupleMemberViewType>
ExprRef<TupleMemberViewType>& OpTupleIndex<TupleMemberViewType>::getMember() {
    debug_code(assert(defined));
    debug_code(assert(index >= 0 && !tupleOperand->isUndefined() &&
                      index < (UInt)tupleOperand->view().members.size()));
    auto& member = mpark::get<ExprRef<TupleMemberViewType>>(
        tupleOperand->view().members[index]);
    debug_code(assert(!member->isUndefined()));
    return member;
}

template <typename TupleMemberViewType>
const ExprRef<TupleMemberViewType>&
OpTupleIndex<TupleMemberViewType>::getMember() const {
    debug_code(assert(defined));
    debug_code(assert(index >= 0 && !tupleOperand->isUndefined() &&
                      index < (UInt)tupleOperand->view().members.size()));
    const auto& member = mpark::get<ExprRef<TupleMemberViewType>>(
        tupleOperand->view().members[index]);
    debug_code(assert(!member->isUndefined()));
    return member;
}

template <typename TupleMemberViewType>
TupleMemberViewType& OpTupleIndex<TupleMemberViewType>::view() {
    return getMember()->view();
}
template <typename TupleMemberViewType>
const TupleMemberViewType& OpTupleIndex<TupleMemberViewType>::view() const {
    return getMember()->view();
}
template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::reevaluateDefined() {
    defined = !tupleOperand->isUndefined();
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::evaluate() {
    tupleOperand->evaluate();
    reevaluateDefined();
}

template <typename TupleMemberViewType>
OpTupleIndex<TupleMemberViewType>::OpTupleIndex(
    OpTupleIndex<TupleMemberViewType>&& other)
    : ExprInterface<TupleMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      tupleOperand(move(other.tupleOperand)),
      index(move(other.index)),
      defined(move(other.defined)),
      tupleTrigger(move(other.tupleTrigger)) {
    setTriggerParent(this, tupleTrigger);
}

template <typename TupleMemberViewType>
struct OpTupleIndex<TupleMemberViewType>::TupleOperandTrigger
    : public TupleTrigger {
    OpTupleIndex* op;
    TupleOperandTrigger(OpTupleIndex* op) : op(op) {}
    void possibleValueChange() final {
        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
    }
    void valueChanged() final {
        visitTriggers([&](auto& t) { t->valueChanged(); }, op->triggers);
    }

    inline void possibleMemberValueChange(UInt index) final {
        if ((UInt)index == op->index) {
            visitTriggers([&](auto& t) { t->possibleValueChange(); },
                          op->triggers);
        }
    }

    inline void memberValueChanged(UInt index) final {
        if ((UInt)index == op->index) {
            visitTriggers([&](auto& t) { t->valueChanged(); }, op->triggers);
        }
    }

    void reattachTrigger() final {
        deleteTrigger(op->tupleTrigger);
        auto trigger =
            make_shared<OpTupleIndex<TupleMemberViewType>::TupleOperandTrigger>(
                op);
        op->tupleOperand->addTrigger(trigger);
        op->tupleTrigger = trigger;
    }

    inline void hasBecomeUndefined() final {
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
};

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::startTriggering() {
    if (!tupleTrigger) {
        tupleTrigger =
            make_shared<OpTupleIndex<TupleMemberViewType>::TupleOperandTrigger>(
                this);
        tupleOperand->addTrigger(tupleTrigger);
        tupleOperand->startTriggering();
    }
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::stopTriggeringOnChildren() {
    if (tupleTrigger) {
        deleteTrigger(tupleTrigger);
        tupleTrigger = nullptr;
    }
}
template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::stopTriggering() {
    if (tupleTrigger) {
        stopTriggeringOnChildren();
        tupleOperand->stopTriggering();
    }
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::updateViolationDescription(
    UInt parentViolation, ViolationDescription& vioDesc) {
    if (defined) {
        getMember()->updateViolationDescription(parentViolation, vioDesc);
    } else {
        tupleOperand->updateViolationDescription(parentViolation, vioDesc);
    }
}

template <typename TupleMemberViewType>
ExprRef<TupleMemberViewType>
OpTupleIndex<TupleMemberViewType>::deepCopySelfForUnroll(
    const ExprRef<TupleMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpTupleIndex = make_shared<OpTupleIndex<TupleMemberViewType>>(
        tupleOperand->deepCopySelfForUnroll(tupleOperand, iterator), index);
    newOpTupleIndex->index = index;
    newOpTupleIndex->defined = defined;
    return newOpTupleIndex;
}

template <typename TupleMemberViewType>
std::ostream& OpTupleIndex<TupleMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opTupleIndex(tuple=";
    tupleOperand->dumpState(os) << ",\n";
    os << "index=" << index << ")";
    return os;
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->tupleOperand = findAndReplace(tupleOperand, func);
}

template <typename TupleMemberViewType>
bool OpTupleIndex<TupleMemberViewType>::isUndefined() {
    return !defined;
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
