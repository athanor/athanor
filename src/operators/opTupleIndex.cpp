#include "operators/opTupleIndex.h"
#include <iostream>
#include <memory>
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::addTriggerImpl(
    const shared_ptr<TupleMemberTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    triggers.emplace_back(getTriggerBase(trigger));
    getMember().get()->addTrigger(trigger, includeMembers, memberIndex);
}

template <typename TupleMemberViewType>
OptionalRef<ExprRef<TupleMemberViewType>>
OpTupleIndex<TupleMemberViewType>::getMember() {
    debug_code(assert(this->appearsDefined()));
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
    debug_code(assert(this->appearsDefined()));
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
        return EmptyOptional();
    }
}
template <typename TupleMemberViewType>
OptionalRef<const TupleMemberViewType> OpTupleIndex<TupleMemberViewType>::view()
    const {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return EmptyOptional();
    }
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::reevaluate() {
    setAppearsDefined(getMember().get()->appearsDefined());
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::evaluateImpl() {
    tupleOperand->evaluate();
    reevaluate();
}

template <typename TupleMemberViewType>
OpTupleIndex<TupleMemberViewType>::OpTupleIndex(
    OpTupleIndex<TupleMemberViewType>&& other)
    : ExprInterface<TupleMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      tupleOperand(move(other.tupleOperand)),
      indexOperand(move(other.indexOperand)),
      tupleOperandTrigger(move(other.tupleOperandTrigger)),
      tupleMemberTrigger(move(other.tupleMemberTrigger)) {
    setTriggerParent(this, tupleOperandTrigger, tupleMemberTrigger);
}

template <typename TupleMemberViewType>
bool OpTupleIndex<TupleMemberViewType>::eventForwardedAsDefinednessChange() {
    bool wasDefined = this->appearsDefined();
    reevaluate();
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
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers);
    }

    void memberHasBecomeDefined(UInt index) final {
        debug_code(assert(index == op->indexOperand));
        ignoreUnused(index);
        op->setAppearsDefined(true);
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers);
    }

    void valueChanged() final {
        if (!op->eventForwardedAsDefinednessChange()) {
            visitTriggers(
                [&](auto& t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
        }
    }

    inline void hasBecomeUndefined() {
        if (!op->appearsDefined()) {
            return;
        }
        op->setAppearsDefined(false);
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers,
                      true);
    }

    void hasBecomeDefined() {
        op->reevaluate();
        if (!op->appearsDefined()) {
            return;
        }
        op->reattachTupleMemberTrigger();
        visitTriggers(
            [&](auto t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            op->triggers, true);
    }
    void reattachTrigger() final {
        deleteTrigger(op->tupleOperandTrigger);
        if (op->tupleMemberTrigger) {
            deleteTrigger(op->tupleMemberTrigger);
        }
        auto trigger = make_shared<TupleOperandTrigger>(op);
        op->tupleOperand->addTrigger(trigger, false);
        op->reattachTupleMemberTrigger();
        op->tupleOperandTrigger = trigger;
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
    tupleMemberTrigger = make_shared<TupleOperandTrigger>(this);
    tupleOperand->addTrigger(tupleMemberTrigger, true, indexOperand);
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::stopTriggeringOnChildren() {
    if (tupleOperandTrigger) {
        deleteTrigger(tupleOperandTrigger);
        deleteTrigger(tupleMemberTrigger);
        tupleOperandTrigger = nullptr;
        tupleMemberTrigger = nullptr;
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
OpTupleIndex<TupleMemberViewType>::deepCopySelfForUnrollImpl(
    const ExprRef<TupleMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpTupleIndex = make_shared<OpTupleIndex<TupleMemberViewType>>(
        tupleOperand->deepCopySelfForUnroll(tupleOperand, iterator),
        indexOperand);
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
OpTupleIndex<TupleMemberViewType>::optimise(PathExtension path) {
    bool changeMade = false;
    auto optResult = tupleOperand->optimise(path.extend(tupleOperand));
    changeMade |= optResult.first;
    tupleOperand = optResult.second;
    return make_pair(changeMade,
                     mpark::get<ExprRef<TupleMemberViewType>>(path.expr));
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
