#include "operators/opSetIndexInternal.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <numeric>
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
ExprRef<SetMemberViewType>& OpSetIndexInternal<SetMemberViewType>::getMember(
    size_t index) {
    debug_code(assert(!setOperand->isUndefined()));
    auto& members = setOperand->view().getMembers<SetMemberViewType>();
    debug_code(assert(index < parentSetMapping.size()));
    debug_code(assert(parentSetMapping[index] < members.size()));
    auto& member = members[parentSetMapping[index]];
    return member;
}

template <typename SetMemberViewType>
const ExprRef<SetMemberViewType>&
OpSetIndexInternal<SetMemberViewType>::getMember(size_t index) const {
    debug_code(assert(!setOperand->isUndefined()));
    const auto& members = setOperand->view().getMembers<SetMemberViewType>();
    debug_code(assert(index < parentSetMapping.size()));
    debug_code(assert(parentSetMapping[index] < members.size()));
    const auto& member = members[parentSetMapping[index]];
    return member;
}

template <typename SetMemberViewType>
ExprRef<SetMemberViewType>& OpSetIndexInternal<SetMemberViewType>::getMember() {
    return getMember(index);
}

template <typename SetMemberViewType>
const ExprRef<SetMemberViewType>&
OpSetIndexInternal<SetMemberViewType>::getMember() const {
    return getMember(index);
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
    defined = !setOperand->isUndefined() &&
              parentSetMapping[index] < setOperand->view().numberElements() &&
              !getMember()->isUndefined();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::evaluateImpl() {
    setOperand->evaluate();

    evaluateMappings();
    reevaluateDefined();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::evaluateMappings() {
    parentSetMapping.resize(setOperand->view().numberElements());
    iota(parentSetMapping.begin(), parentSetMapping.end(), 0);
    sort(parentSetMapping.begin(), parentSetMapping.end(),
         [&](size_t u, size_t v) {
             return smallerValue(getMember(u)->view(), getMember(v)->view());
         });
    setParentMapping.resize(parentSetMapping.size());
    for (size_t i = 0; i < parentSetMapping.size(); i++) {
        setParentMapping[parentSetMapping[i]] = i;
    }
}

template <typename SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::OpSetIndexInternal(
    OpSetIndexInternal<SetMemberViewType>&& other)
    : ExprInterface<SetMemberViewType>(move(other)),
      triggers(move(other.triggers)),
      setOperand(move(other.setOperand)),
      index(move(other.index)),
      defined(move(other.defined)),
      setTrigger(move(other.setTrigger)),
      parentSetMapping(move(other.parentSetMapping)),
      setParentMapping(move(other.setParentMapping)) {
    setTriggerParent(this, setTrigger);
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::handleSetMemberValueChange(
    UInt memberIndex) {
    size_t index = setParentMapping[memberIndex];
    visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);

    int increment;
    if (index == 0) {
        increment = 1;
    } else if (index == setOperand->view().numberElements() - 1) {
        increment = -1;
    } else {
        increment = (smallerValue(getMember(index - 1)->view(),
                                  getMember(index)->view()))
                        ? 1
                        : -1;
    }
    size_t limit = (increment == 1) ? setOperand->view().numberElements() : 0;
    bool shouldBeSmaller = (increment == 1) ? false : true;
    for (size_t i = index;
         i != limit &&
         smallerValue(getMember(index + increment)->view(),
                      getMember(index)->view()) != shouldBeSmaller;
         i = i + increment) {
        swapMemberMappings(index + increment, index);
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::swapMemberMappings(size_t index1,
                                                               size_t index2) {
    size_t temp = parentSetMapping[index1];
    if (index1 == index) {
        notifyPossibleMemberSwap();
    }
    parentSetMapping[index1] = parentSetMapping[index2];
    if (index1 == index) {
        notifyMemberSwapped();
    }

    if (index2 == index) {
        notifyPossibleMemberSwap();
    }
    parentSetMapping[index2] = temp;
    if (index2 == index) {
        notifyMemberSwapped();
    }

    swap(setParentMapping[parentSetMapping[index1]],
         setParentMapping[parentSetMapping[index2]]);
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::notifyPossibleMemberSwap() {
    visitTriggers([&](auto& t) { t->possibleValueChange(); }, triggers);
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::notifyMemberSwapped() {
    visitTriggers(
        [&](auto& t) {
            t->valueChanged();
            t->reattachTrigger();
        },
        triggers);
}

template <typename SetMemberViewType>
struct OpSetIndexInternal<SetMemberViewType>::SetOperandTrigger
    : public SetTrigger {
    OpSetIndexInternal<SetMemberViewType>* op;
    SetOperandTrigger(OpSetIndexInternal<SetMemberViewType>* op) : op(op) {}

    inline void hasBecomeUndefined() final { todoImpl(); }

    void hasBecomeDefined() final { todoImpl(); }

    void possibleValueChange() final {
        visitTriggers([&](auto& t) { t->possibleValueChange(); }, op->triggers);
    }

    void valueChanged() final {
        op->evaluateMappings();
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            op->triggers);
    }
    inline void possibleMemberValueChange(UInt, const AnyExprRef&) final {
        // since the parent will already be directly triggering on the set
        // member, this trigger need not be forwarded
    }

    inline void memberValueChanged(UInt index, const AnyExprRef&) final {
        op->handleSetMemberValueChange(index);
    }

    void reattachTrigger() final {
        deleteTrigger(op->setTrigger);
        auto trigger = make_shared<
            OpSetIndexInternal<SetMemberViewType>::SetOperandTrigger>(op);
        op->setOperand->addTrigger(trigger);
        op->setTrigger = trigger;
    }

    void valueAdded(const AnyExprRef&) { todoImpl(); }
    void valueRemoved(UInt, HashType) final { todoImpl(); }
};

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::startTriggeringImpl() {
    if (!setTrigger) {
        setTrigger = make_shared<
            typename OpSetIndexInternal<SetMemberViewType>::SetOperandTrigger>(
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
    auto newOpSetIndex = make_shared<OpSetIndexInternal<SetMemberViewType>>(
        setOperand->deepCopySelfForUnroll(setOperand, iterator), index);
    newOpSetIndex->parentSetMapping = parentSetMapping;
    newOpSetIndex->setParentMapping = setParentMapping;
    newOpSetIndex->defined = defined;
    return newOpSetIndex;
}

template <typename SetMemberViewType>
std::ostream& OpSetIndexInternal<SetMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opSetIndexInternal(set=";
    setOperand->dumpState(os) << ",\n";
    os << "index=" << index << ",\nparentSetMapping=" << parentSetMapping
       << ",\nsetParentMapping=" << setParentMapping << ")";
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
pair<bool, ExprRef<SetMemberViewType>>
OpSetIndexInternal<SetMemberViewType>::optimise(PathExtension path) {
    auto optResult = setOperand->optimise(path.extend(setOperand));
    setOperand = optResult.second;
    return make_pair(optResult.first,
                     mpark::get<ExprRef<SetMemberViewType>>(path.expr));
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
