#include "operators/opSetIndexInternal.h"

#include <iostream>
#include <memory>

#include "operators/emptyOrViolatingOptional.h"
#include "triggers/allTriggers.h"
#include "types/set.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::addTriggerImpl(
    const shared_ptr<SetMemberTriggerType>& trigger, bool includeMembers,
    Int memberIndex) {
    handleTriggerAdd(trigger, includeMembers, memberIndex, *this);
}

template <typename SetMemberViewType>
OptionalRef<ExprRef<SetMemberViewType>>
OpSetIndexInternal<SetMemberViewType>::getMember() {
    auto view = setOperand->getViewIfDefined();
    if (!view) {
        return EmptyOptional();
    }
    if (indexOperand >= elementOrder.size()) {
        return EmptyOptional();
    }
    return view
        ->template getMembers<SetMemberViewType>()[elementOrder[indexOperand]];
}

template <typename SetMemberViewType>
OptionalRef<const ExprRef<SetMemberViewType>>
OpSetIndexInternal<SetMemberViewType>::getMember() const {
    auto view = setOperand->getViewIfDefined();
    if (!view) {
        return EmptyOptional();
    }
    if (indexOperand >= elementOrder.size()) {
        return EmptyOptional();
    }
    return view
        ->template getMembers<SetMemberViewType>()[elementOrder[indexOperand]];
}

template <typename SetMemberViewType>
OptionalRef<SetMemberViewType> OpSetIndexInternal<SetMemberViewType>::view() {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return emptyOrViolatingOptional<SetMemberViewType>();
    }
}
template <typename SetMemberViewType>
OptionalRef<const SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::view() const {
    auto member = getMember();
    if (member) {
        return (*member)->view();
    } else {
        return emptyOrViolatingOptional<SetMemberViewType>();
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::reevaluate() {
    auto view = setOperand->getViewIfDefined();
    exprDefined = false;
    isLastElementInSet = false;
    elementOrder.clear();
    if (view && indexOperand < view->numberElements()) {
        elementOrder.resize(view->numberElements());
        iota(begin(elementOrder), end(elementOrder), 0);
        sort(begin(elementOrder), end(elementOrder), [&](auto& i, auto& j) {
            auto& view1 =
                *view->getMembers<SetMemberViewType>()[i]->getViewIfDefined();
            auto& view2 =
                *view->getMembers<SetMemberViewType>()[j]->getViewIfDefined();
            return getValueHash(view1) < getValueHash(view2);
        });
        exprDefined = true;
        isLastElementInSet =
            elementOrder[indexOperand] + 1 == view->numberElements();
    }
    setAppearsDefined(exprDefined);
}
template <typename SetMemberViewType>
bool OpSetIndexInternal<SetMemberViewType>::allowForwardingOfTrigger() {
    return true;
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::evaluateImpl() {
    setOperand->evaluate();
    reevaluate();
}

template <typename SetMemberViewType>
bool OpSetIndexInternal<
    SetMemberViewType>::eventForwardedAsDefinednessChange() {
    bool wasDefined = exprDefined;
    reevaluate();
    if (wasDefined && !exprDefined) {
        deleteTrigger(setMemberTrigger);
        setMemberTrigger = nullptr;
        deleteTrigger(memberTrigger);
        memberTrigger = nullptr;

        if (is_same<BoolView, SetMemberViewType>::value) {
            this->notifyEntireValueChanged();
        } else {
            this->notifyValueUndefined();
        }
        return true;
    } else if (!wasDefined && exprDefined) {
        if (is_same<BoolView, SetMemberViewType>::value) {
            this->notifyEntireValueChanged();
        } else {
            this->notifyValueDefined();
        }
        reattachSetMemberTrigger();
        return true;
    } else {
        return !exprDefined;
    }
}

template <typename SetMemberViewType>
struct OpSetIndexInternal<SetMemberViewType>::SetOperandTrigger
    : public SetTrigger {
    OpSetIndexInternal<SetMemberViewType>* op;
    SetOperandTrigger(OpSetIndexInternal<SetMemberViewType>* op) : op(op) {}
    void valueRemoved(UInt indexOfRemovedValue, HashType) {
        size_t memberUnderFocus = (op->indexOperand < op->elementOrder.size())
                                      ? op->elementOrder[op->indexOperand]
                                      : 0;
        if (!op->eventForwardedAsDefinednessChange()) {
            if (op->isLastElementInSet ||  // was pointing to last element which
                                           // is moved after a set remove
                indexOfRemovedValue ==
                    op->elementOrder[op->indexOperand] ||  // was pointing to
                                                           // the removed
                                                           // element
                op->elementOrder[op->indexOperand] !=
                    memberUnderFocus  // pointing to different element
            ) {
                op->reattachSetMemberTrigger();
                op->notifyEntireValueChanged();
            }
        }
    }
    inline void triggerIfChanged() {
        size_t memberUnderFocus = (op->indexOperand < op->elementOrder.size())
                                      ? op->elementOrder[op->indexOperand]
                                      : 0;
        if (!op->eventForwardedAsDefinednessChange()) {
            if (op->elementOrder[op->indexOperand] != memberUnderFocus) {
                op->reattachSetMemberTrigger();
                op->notifyEntireValueChanged();
            }
        }
    }
    void valueAdded(const AnyExprRef&) { triggerIfChanged(); }

    void memberValueChanged(UInt, HashType) { triggerIfChanged(); }

    void memberValuesChanged(const std::vector<UInt>&,
                             const std::vector<HashType>&) {
        triggerIfChanged();
    }

    void memberReplaced(UInt indexOfReplacedMember, const AnyExprRef&) {
        size_t memberUnderFocus = (op->indexOperand < op->elementOrder.size())
                                      ? op->elementOrder[op->indexOperand]
                                      : 0;
        if (!op->eventForwardedAsDefinednessChange()) {
            if (op->elementOrder[op->indexOperand] != memberUnderFocus ||
                memberUnderFocus == indexOfReplacedMember) {
                op->reattachSetMemberTrigger();
                op->notifyEntireValueChanged();
            }
        }
    }
    void valueChanged() final {
        if (!op->eventForwardedAsDefinednessChange()) {
            op->notifyEntireValueChanged();
            op->reattachSetMemberTrigger();
        }
    }

    inline void hasBecomeUndefined() {
        op->eventForwardedAsDefinednessChange();
    }

    void hasBecomeDefined() { op->eventForwardedAsDefinednessChange(); }
    void reattachTrigger() final {
        deleteTrigger(op->setOperandTrigger);
        deleteTrigger(op->setMemberTrigger);
        deleteTrigger(op->memberTrigger);
        auto trigger = make_shared<SetOperandTrigger>(op);
        op->setOperand->addTrigger(trigger, false);
        op->reattachSetMemberTrigger();
        op->setOperandTrigger = trigger;
    }
};

template <typename SetMemberViewType>
struct OpSetIndexInternal<SetMemberViewType>::MemberTrigger
    : public ForwardingTrigger<
          typename AssociatedTriggerType<SetMemberViewType>::type,
          OpSetIndexInternal<SetMemberViewType>,
          typename OpSetIndexInternal<SetMemberViewType>::MemberTrigger> {
    using ForwardingTrigger<
        typename AssociatedTriggerType<SetMemberViewType>::type,
        OpSetIndexInternal<SetMemberViewType>,
        typename OpSetIndexInternal<SetMemberViewType>::MemberTrigger>::
        ForwardingTrigger;
    ExprRef<SetMemberViewType>& getTriggeringOperand() {
        return this->op->getMember().get();
    }
    void reattachTrigger() final {
        deleteTrigger(this->op->memberTrigger);
        auto trigger = make_shared<
            typename OpSetIndexInternal<SetMemberViewType>::MemberTrigger>(
            this->op);
        this->op->getMember().get()->addTrigger(trigger);
        this->op->memberTrigger = trigger;
    }
};

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::startTriggeringImpl() {
    if (!setOperandTrigger) {
        setOperandTrigger = make_shared<
            OpSetIndexInternal<SetMemberViewType>::SetOperandTrigger>(this);
        setOperand->addTrigger(setOperandTrigger, false);
        if (exprDefined) {
            reattachSetMemberTrigger();
        }
        setOperand->startTriggering();
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::reattachSetMemberTrigger() {
    deleteTrigger(setMemberTrigger);
    deleteTrigger(memberTrigger);

    setMemberTrigger = make_shared<SetOperandTrigger>(this);
    memberTrigger =
        make_shared<OpSetIndexInternal<SetMemberViewType>::MemberTrigger>(this);
    if (exprDefined) {
        getMember().get()->addTrigger(memberTrigger);
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::stopTriggeringOnChildren() {
    if (setOperandTrigger) {
        deleteTrigger(setOperandTrigger);
        deleteTrigger(setMemberTrigger);
        deleteTrigger(memberTrigger);

        setOperandTrigger = nullptr;
        setMemberTrigger = nullptr;
        memberTrigger = nullptr;
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::stopTriggering() {
    if (setOperandTrigger) {
        stopTriggeringOnChildren();
        setOperand->stopTriggering();
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    auto setMember = getMember();
    if (setMember) {
        (*setMember)->updateVarViolations(vioContext, vioContainer);
    } else {
        setOperand->updateVarViolations(vioContext, vioContainer);
    }
}

template <typename SetMemberViewType>
ExprRef<SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::deepCopyForUnrollImpl(
    const ExprRef<SetMemberViewType>&, const AnyIterRef& iterator) const {
    auto newOpSetIndexInternal =
        make_shared<OpSetIndexInternal<SetMemberViewType>>(
            setOperand->deepCopyForUnroll(setOperand, iterator), indexOperand);
    return newOpSetIndexInternal;
}

template <typename SetMemberViewType>
std::ostream& OpSetIndexInternal<SetMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opSetIndexInternal(value=";
    prettyPrint(os, this->getViewIfDefined());
    os << ",\n";

    os << "set=";
    setOperand->dumpState(os) << ",\n";
    os << "index=";
    os << indexOperand << ")";
    return os;
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func, PathExtension path) {
    this->setOperand = findAndReplace(setOperand, func, path);
}

template <typename SetMemberViewType>
pair<bool, ExprRef<SetMemberViewType>>
OpSetIndexInternal<SetMemberViewType>::optimiseImpl(ExprRef<SetMemberViewType>&,
                                                    PathExtension path) {
    bool optimised = false;
    auto newOp = make_shared<OpSetIndexInternal<SetMemberViewType>>(
        setOperand, indexOperand);
    AnyExprRef newOpAsExpr = ExprRef<SetMemberViewType>(newOp);
    optimised |= optimise(newOpAsExpr, newOp->setOperand, path);
    return make_pair(optimised, newOp);
}

template <typename SetMemberViewType>
string OpSetIndexInternal<SetMemberViewType>::getOpName() const {
    return toString(
        "OpSetIndexInternal<",
        TypeAsString<
            typename AssociatedValueType<SetMemberViewType>::type>::value,
        ">");
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::debugSanityCheckImpl() const {
    setOperand->debugSanityCheck();
    if (!setOperand->appearsDefined()) {
        sanityCheck(!this->appearsDefined(),
                    "operator should be undefined as at least one operand is "
                    "undefined.");
        return;
    }
    if (!(*getMember())->appearsDefined()) {
        sanityCheck(!this->appearsDefined(),
                    "member pointed to by index is undefined, hence this "
                    "operator should be undefined.");
        return;
    }
    sanityCheck(this->appearsDefined(), "operator should be defined.");
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
