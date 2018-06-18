#include "operators/opSetIndexInternal.h"
#include <iostream>
#include <memory>
#include "utils/ignoreUnused.h"
using namespace std;

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::addTriggerImpl(
    const shared_ptr<SetMemberTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    triggers.emplace_back(getTriggerBase(trigger));
    if (!setOperand->isUndefined()) {
        getMember()->addTrigger(trigger, includeMembers, memberIndex);
    }
}

template <typename SetMemberViewType>
ExprRef<SetMemberViewType>& OpSetIndexInternal<SetMemberViewType>::getMember() {
    debug_code(assert(!setOperand->isUndefined()));
    auto& members = setOperand->view().getMembers<SetMemberViewType>();
    debug_code(assert(index < members.size()));
    auto& member =
        members[index];
    return member;
}


template <typename SetMemberViewType>
const ExprRef<SetMemberViewType>& OpSetIndexInternal<SetMemberViewType>::getMember() const {
    debug_code(assert(!setOperand->isUndefined()));
    const auto& members = setOperand->view().getMembers<SetMemberViewType>();
    debug_code(assert(index < members.size()));
    const auto& member =
        members[index];
    return member;
}


template <typename SetMemberViewType>
SetMemberViewType& OpSetIndexInternal<SetMemberViewType>::view() {
    return getMember()->view();
}
template <typename SetMemberViewType>
const SetMemberViewType& OpSetIndexInternal<SetMemberViewType>::view() const {
    return getMember()->view();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::reevaluateDefined() {
    defined = !setOperand->isUndefined() && index < setOperand->view().numberElements() && !getMember()->isUndefined();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::evaluateImpl() {
    setOperand->evaluate();
    reevaluateDefined();
}

template <typename SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::OpSetIndexInternal(
    OpSetIndexInternal<SetMemberViewType>&& other)
    : ExprInterface<SetMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      setOperand(move(other.setOperand)),
      index(move(other.index)),
      defined(move(other.defined)),
      setTrigger(move(other.setTrigger)) {
    setTriggerParent(this, setTrigger);
}

template <typename SetMemberViewType>
struct OpSetIndexInternal<SetMemberViewType>::SetOperandTrigger
    : public SetTrigger {
    OpSetIndexInternal<SetMemberViewType>* op;
    SetOperandTrigger(OpSetIndexInternal<SetMemberViewType>* op) : op(op) {}

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

    inline void possibleMemberValueChange(UInt, const AnyExprRef&) final {
        // since the parent will already be directly triggering on the set
        // member, this trigger need not be forwarded
    }

    inline void memberValueChanged(UInt, const AnyExprRef&) final {
        // since the parent will already be directly triggering on the set
        // member, this trigger need not be forwarded
    }

    void reattachTrigger() final {
        deleteTrigger(op->setTrigger);
        auto trigger =
            make_shared<OpSetIndexInternal<SetMemberViewType>::SetOperandTrigger>(
                op);
        op->setOperand->addTrigger(trigger);
        op->setTrigger = trigger;
    }

void valueAdded(const AnyExprRef&) {
if (!op->defined) {
    eventForwardedAsDefinednessChange();
}
}
void valueRemoved(UInt index, HashType) final  {
    if (index == op->index) {
        valueChanged();
    }
}
};

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::startTriggeringImpl() {
    if (!setTrigger) {
        setTrigger =
            make_shared<OpSetIndexInternal<SetMemberViewType>::SetOperandTrigger>(
                this);
        setOperand->addTrigger(setTrigger);
        setOperand->startTriggering();
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::stopTriggeringOnChildren() {
    if (setTrigger) {
        deleteTrigger(setTrigger);
        setTrigger = nullptr;
    }
}
template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::stopTriggering() {
    if (setTrigger) {
        stopTriggeringOnChildren();
        setOperand->stopTriggering();
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::updateVarViolations(
    const ViolationContext& vioContext, ViolationContainer& vioDesc) {
    if (defined) {
        getMember()->updateVarViolations(vioContext, vioDesc);
    } else {
        setOperand->updateVarViolations(vioContext, vioDesc);
    }
}

template <typename SetMemberViewType>
ExprRef<SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::deepCopySelfForUnrollImpl(
    const ExprRef<SetMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpSetIndexInternal = make_shared<OpSetIndexInternal<SetMemberViewType>>(
        setOperand->deepCopySelfForUnroll(setOperand, iterator), index);
    newOpSetIndexInternal->defined = defined;
    return newOpSetIndexInternal;
}

template <typename SetMemberViewType>
std::ostream& OpSetIndexInternal<SetMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opSetIndexInternal(set=";
    setOperand->dumpState(os) << ",\n";
    os << "index=" << index << ")";
    return os;
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->setOperand = findAndReplace(setOperand, func);
}

template <typename SetMemberViewType>
bool OpSetIndexInternal<SetMemberViewType>::isUndefined() {
    return !defined;
}

template <typename SetMemberViewType>
bool OpSetIndexInternal<SetMemberViewType>::optimise(PathExtension path) {
    return setOperand->optimise(path.extend(setOperand));
}

template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpSetIndexInternal<View>> {
    static ExprRef<View> make(ExprRef<SetView> set, UInt index);
};

template <typename View>
ExprRef<View> OpMaker<OpSetIndexInternal<View>>::make(ExprRef<SetView> set,
                                                UInt index) {
    return make_shared<OpSetIndexInternal<View>>(move(set), index);
}

#define opSetIndexInternalInstantiators(name)       \
    template struct OpSetIndexInternal<name##View>; \
    template struct OpMaker<OpSetIndexInternal<name##View>>;

buildForAllTypes(opSetIndexInternalInstantiators, );
#undef opSetIndexInternalInstantiators
