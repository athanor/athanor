#include "operators/opTupleIndex.h"
#include <iostream>
#include <memory>
#include "utils/ignoreUnused.h"

using namespace std;

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::addTrigger(
    const shared_ptr<TupleMemberTriggerType>& trigger) {
    auto& member = mpark::get<ExprRef<TupleMemberViewType>>(
        tupleOperand->view().members[index]);
    member->addTrigger(trigger);
    triggers.emplace_back(getTriggerBase(trigger));
}

template <typename TupleMemberViewType>
TupleMemberViewType& OpTupleIndex<TupleMemberViewType>::view() {
    auto& member = mpark::get<ExprRef<TupleMemberViewType>>(
        tupleOperand->view().members[index]);
    return member->view();
}

template <typename TupleMemberViewType>
const TupleMemberViewType& OpTupleIndex<TupleMemberViewType>::view() const {
    const auto& member = mpark::get<ExprRef<TupleMemberViewType>>(
        tupleOperand->view().members[index]);
    return member->view();
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::evaluate() {
    tupleOperand->evaluate();
}

template <typename TupleMemberViewType>
OpTupleIndex<TupleMemberViewType>::OpTupleIndex(
    OpTupleIndex<TupleMemberViewType>&& other)
    : ExprInterface<TupleMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      tupleOperand(move(other.tupleOperand)),
      index(move(other.index)),
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
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            op->triggers);
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
        if (!op->defined) {
            return;
        }
        op->defined = false;
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers);
    }
    void hasBecomeDefined() final {
        if (op->indexOperand->isUndefined()) {
            return;
        }
        op->defined =
            op->cachedIndex >= 0 &&
            op->cachedIndex < (Int)op->sequenceOperand->view().numberElements();
        if (!op->defined) {
            return;
        }
        visitTriggers(
            [&](auto& t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            op->triggers);
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
    auto& member = mpark::get<ExprRef<TupleMemberViewType>>(
        tupleOperand->view().members[index]);
    member->updateViolationDescription(parentViolation, vioDesc);
}

template <typename TupleMemberViewType>
ExprRef<TupleMemberViewType>
OpTupleIndex<TupleMemberViewType>::deepCopySelfForUnroll(
    const ExprRef<TupleMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpTupleIndex = make_shared<OpTupleIndex<TupleMemberViewType>>(
        tupleOperand->deepCopySelfForUnroll(tupleOperand, iterator), index);
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
    if (tupleOperand->isUndefined()) {
        return true;
    }
    auto& member = mpark::get<ExprRef<TupleMemberViewType>>(
        tupleOperand->view().members[index]);
    return member->isUndefined();
}(

template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpTupleIndex<View>> {
    static ExprRef<View> make(ExprRef<TupleView> tuple, UInt index);
};

template <typename View>
ExprRef<View> OpMaker<OpTupleIndex<View>>::make(ExprRef<TupleView> tuple,
                                                UInt index) {
    return make_shared<OpTupleIndex<View>>(move(tuple), move(index));
}

#define opTupleIndexInstantiators(name)       \
    template struct OpTupleIndex<name##View>; \
    template struct OpMaker<OpTupleIndex<name##View>>;

buildForAllTypes(opTupleIndexInstantiators, );
#undef opTupleIndexInstantiators
