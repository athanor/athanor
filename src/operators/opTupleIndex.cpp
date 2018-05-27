#include "operators/opTupleIndex.h"
#include <iostream>
#include <memory>
#include "utils/ignoreUnused.h"
using namespace std;

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::addTrigger(
    const shared_ptr<TupleMemberTriggerType>& trigger, bool includeMembers) {
    triggers.emplace_back(getTriggerBase(trigger));
    if (!tupleOperand->isUndefined()) {
        getMember()->addTrigger(trigger, includeMembers);
    }
}

template <typename TupleMemberViewType>
ExprRef<TupleMemberViewType>& OpTupleIndex<TupleMemberViewType>::getMember() {
    debug_code(assert(!tupleOperand->isUndefined()));
    auto& member = mpark::get<ExprRef<TupleMemberViewType>>(
        tupleOperand->view().members[index]);
    debug_code(assert(!member->isUndefined()));
    return member;
}

template <typename TupleMemberViewType>
const ExprRef<TupleMemberViewType>&
OpTupleIndex<TupleMemberViewType>::getMember() const {
    debug_code(assert(!tupleOperand->isUndefined()));

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
    defined = !tupleOperand->isUndefined() && !getMember()->isUndefined();
}

template <typename TupleMemberViewType>
void OpTupleIndex<TupleMemberViewType>::evaluateImpl() {
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
    OpTupleIndex<TupleMemberViewType>* op;
    TupleOperandTrigger(OpTupleIndex<TupleMemberViewType>* op) : op(op) {}

    bool eventForwardedAsDefinednessChange() {
        bool wasDefined = op->defined;
        op->reevaluateDefined();
        if (wasDefined && !op->defined) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
            return true;
        } else if (!wasDefined && op->defined) {
            visitTriggers(
                [&](auto& t) {
                    t->hasBecomeDefined();
                    t->reattachTrigger();
                },
                op->triggers);
            return true;
        } else {
            return !op->defined;
        }
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
        visitTriggers(
            [&](auto& t) {
                t->hasBecomeDefined();
                t->reattachTrigger();
            },
            op->triggers);
    }

    void possibleValueChange() final {
        if (!op->defined) {
            return;
        }

        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
    }
    void valueChanged() final {
        if (!eventForwardedAsDefinednessChange()) {
            visitTriggers(
                [&](auto& t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                op->triggers);
        }
    }

    inline void possibleMemberValueChange(UInt) final {
        // since the parent will already be directly triggering on the tuple
        // member, this trigger need not be forwarded
    }

    inline void memberValueChanged(UInt) final {
        // since the parent will already be directly triggering on the tuple
        // member, this trigger need not be forwarded
    }

    void reattachTrigger() final {
        deleteTrigger(op->tupleTrigger);
        auto trigger =
            make_shared<OpTupleIndex<TupleMemberViewType>::TupleOperandTrigger>(
                op);
        op->tupleOperand->addTrigger(trigger);
        op->tupleTrigger = trigger;
    }

    void memberHasBecomeUndefined(UInt index) final {
        if (index != op->index) {
            return;
        }
        debug_code(assert(op->defined));
        op->defined = false;
    }

    void memberHasBecomeDefined(UInt index) final {
        if (index != op->index) {
            return;
        }
        debug_code(assert(!op->defined));
        op->defined = true;
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
void OpTupleIndex<TupleMemberViewType>::updateVarViolations(
    const ViolationContext& vioContext, ViolationContainer& vioDesc) {
    if (defined) {
        getMember()->updateVarViolations(vioContext, vioDesc);
    } else {
        tupleOperand->updateVarViolations(vioContext, vioDesc);
    }
}

template <typename TupleMemberViewType>
ExprRef<TupleMemberViewType>
OpTupleIndex<TupleMemberViewType>::deepCopySelfForUnroll(
    const ExprRef<TupleMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpTupleIndex = make_shared<OpTupleIndex<TupleMemberViewType>>(
        tupleOperand->deepCopySelfForUnroll(tupleOperand, iterator), index);
    newOpTupleIndex->defined = defined;
    newOpTupleIndex->evaluated = this->evaluated;
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

template <typename TupleMemberViewType>
bool OpTupleIndex<TupleMemberViewType>::optimise(PathExtension path) {
    return tupleOperand->optimise(path.extend(tupleOperand));
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
