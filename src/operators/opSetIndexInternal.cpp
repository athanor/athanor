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
    if (!sortedSet->setOperand->isUndefined()) {
        getMember()->addTrigger(trigger, includeMembers, memberIndex);
    }
}

template <typename SetMemberViewType>
ExprRef<SetMemberViewType>&
OpSetIndexInternal<SetMemberViewType>::SortedSet::getMember(size_t index) {
    debug_code(assert(!setOperand->isUndefined()));
    auto& members = setOperand->view().getMembers<SetMemberViewType>();
    debug_code(assert(index < parentSetMapping.size()));
    debug_code(assert(parentSetMapping[index] < members.size()));
    auto& member = members[parentSetMapping[index]];
    return member;
}

template <typename SetMemberViewType>
const ExprRef<SetMemberViewType>&
OpSetIndexInternal<SetMemberViewType>::SortedSet::getMember(
    size_t index) const {
    debug_code(assert(!setOperand->isUndefined()));
    const auto& members = setOperand->view().getMembers<SetMemberViewType>();
    debug_code(assert(index < parentSetMapping.size()));
    debug_code(assert(parentSetMapping[index] < members.size()));
    const auto& member = members[parentSetMapping[index]];
    return member;
}

template <typename SetMemberViewType>
ExprRef<SetMemberViewType>& OpSetIndexInternal<SetMemberViewType>::getMember() {
    return sortedSet->getMember(index);
}

template <typename SetMemberViewType>
const ExprRef<SetMemberViewType>&
OpSetIndexInternal<SetMemberViewType>::getMember() const {
    return sortedSet->getMember(index);
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
    defined = !sortedSet->setOperand->isUndefined() &&
              sortedSet->parentSetMapping[index] <
                  sortedSet->setOperand->view().numberElements() &&
              !getMember()->isUndefined();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::evaluateImpl() {
    sortedSet->evaluate();
    reevaluateDefined();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::SortedSet::evaluate() {
    if (evaluated) {
        return;
    }
    evaluated = true;
    setOperand->evaluate();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::SortedSet::reevaluate() {
    parentSetMapping.resize(parents.size());
    iota(parentSetMapping.begin(), parentSetMapping.end(), 0);
    sort(parentSetMapping.begin(), parentSetMapping.end(),
         [&](size_t u, size_t v) {
             return smallerValue(getMember(u)->view(), getMember(v)->view());
             setParentMapping.resize(parentSetMapping.size());
             for (size_t i = 0; i < parentSetMapping.size(); i++) {
                 setParentMapping[parentSetMapping[i]] = i;
             }
         });
}

template <typename SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::OpSetIndexInternal(
    OpSetIndexInternal<SetMemberViewType>&& other)
    : ExprInterface<SetMemberViewType>(move(other)),
      sortedSet(move(other.sortedSet)),
      triggers(move(other.triggers)),
      index(move(other.index)),
      defined(move(other.defined)) {
    sortedSet->parents[index] = this;
}

template <typename SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::SortedSet::SortedSet(
    OpSetIndexInternal<SetMemberViewType>::SortedSet&& other)
    : setOperand(move(other.setOperand)),
      setTrigger(move(other.setTrigger)),
      parents(move(other.parents)),
      parentSetMapping(move(other.parentSetMapping)) {
    setTriggerParent(this, setTrigger);
}

template <typename SetMemberViewType>
void OpSetIndexInternal<
    SetMemberViewType>::SortedSet::notifySetMemberValueChange(UInt index) {
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
        swapMembers(index + increment, index);
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::SortedSet::swapMembers(
    size_t index1, size_t index2) {
    swap(parentSetMapping[index1], parentSetMapping[index2]);
    swap(setParentMapping[parentSetMapping[index1]],
         setParentMapping[parentSetMapping[index2]]);
    parents[index1]->notifyMemberSwapped();
    parents[index2]->notifyMemberSwapped();
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
struct OpSetIndexInternal<SetMemberViewType>::SortedSet::SetOperandTrigger
    : public SetTrigger {
    OpSetIndexInternal<SetMemberViewType>::SortedSet* op;
    SetOperandTrigger(OpSetIndexInternal<SetMemberViewType>::SortedSet* op)
        : op(op) {}

    inline void hasBecomeUndefined() final { todoImpl(); }

    void hasBecomeDefined() final { todoImpl(); }

    void possibleValueChange() final {
        for (auto& parent : op->parents) {
            visitTriggers([&](auto& t) { t->possibleValueChange(); },
                          parent->triggers);
        }
    }
    void valueChanged() final {
        op->reevaluate();
        for (auto& parent : op->parents) {
            visitTriggers(
                [&](auto& t) {
                    t->valueChanged();
                    t->reattachTrigger();
                },
                parent->triggers);
        }
    }

    inline void possibleMemberValueChange(UInt, const AnyExprRef&) final {
        // since the parent will already be directly triggering on the set
        // member, this trigger need not be forwarded
    }

    inline void memberValueChanged(UInt index, const AnyExprRef&) final {
        op->notifySetMemberValueChange(index);
    }

    void reattachTrigger() final {
        deleteTrigger(op->setTrigger);
        auto trigger = make_shared<OpSetIndexInternal<
            SetMemberViewType>::SortedSet::SetOperandTrigger>(op);
        op->setOperand->addTrigger(trigger);
        op->setTrigger = trigger;
    }

    void valueAdded(const AnyExprRef&) { todoImpl(); }
    void valueRemoved(UInt, HashType) final { todoImpl(); }
};

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::startTriggeringImpl() {
    if (!sortedSet->setTrigger) {
        sortedSet->setTrigger = make_shared<typename OpSetIndexInternal<
            SetMemberViewType>::SortedSet::SetOperandTrigger>(
            &(*(this->sortedSet)));
        sortedSet->setOperand->addTrigger(sortedSet->setTrigger);
        sortedSet->setOperand->startTriggering();
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::stopTriggeringOnChildren() {
    if (sortedSet->setTrigger) {
        deleteTrigger(sortedSet->setTrigger);
        sortedSet->setTrigger = nullptr;
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::stopTriggering() {
    if (sortedSet->setTrigger) {
        stopTriggeringOnChildren();
        sortedSet->setOperand->stopTriggering();
    }
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::updateVarViolations(
    const ViolationContext& vioContext, ViolationContainer& vioDesc) {
    if (defined) {
        getMember()->updateVarViolations(vioContext, vioDesc);
    } else {
        sortedSet->setOperand->updateVarViolations(vioContext, vioDesc);
    }
}

template <typename SetMemberViewType>
ExprRef<SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::deepCopySelfForUnrollImpl(
    const ExprRef<SetMemberViewType>&, const AnyIterRef& iterator) const {
    if (!sortedSet->otherSortedSet) {
        sortedSet->otherSortedSet = make_shared<
            typename OpSetIndexInternal<SetMemberViewType>::SortedSet>(
            sortedSet->setOperand->deepCopySelfForUnroll(sortedSet->setOperand,
                                                         iterator));
        sortedSet->otherSortedSet->parents.resize(sortedSet->parents.size());
        sortedSet->otherSortedSet->evaluated = sortedSet->evaluated;
        sortedSet->otherSortedSet->parentSetMapping =
            sortedSet->parentSetMapping;
        sortedSet->otherSortedSet->setParentMapping =
            sortedSet->setParentMapping;
    }
    auto newOpSetIndex = make_shared<OpSetIndexInternal<SetMemberViewType>>(
        sortedSet->otherSortedSet, index);
    newOpSetIndex->sortedSet->parents[index] = &(*newOpSetIndex);
    if (++sortedSet->parentCopyCount == sortedSet->parents.size()) {
        sortedSet->otherSortedSet = nullptr;
    }
    newOpSetIndex->defined = defined;
    return newOpSetIndex;
}

template <typename SetMemberViewType>
std::ostream& OpSetIndexInternal<SetMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opSetIndexInternal(set=";
    sortedSet->setOperand->dumpState(os) << ",\n";
    os << "index=" << index
       << ",\nparentSetMapping=" << sortedSet->parentSetMapping
       << ",\nsetParentMapping=" << sortedSet->setParentMapping << ")";
    return os;
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->sortedSet->setOperand = findAndReplace(sortedSet->setOperand, func);
}

template <typename SetMemberViewType>
bool OpSetIndexInternal<SetMemberViewType>::isUndefined() {
    return !defined;
}

template <typename SetMemberViewType>
bool OpSetIndexInternal<SetMemberViewType>::optimise(PathExtension path) {
    return sortedSet->setOperand->optimise(path.extend(sortedSet->setOperand));
}

template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpSetIndexInternal<View>> {
    static ExprRef<View> make(
        std::shared_ptr<typename OpSetIndexInternal<View>::SortedSet> sortedSet,
        UInt index);
};

template <typename View>
ExprRef<View> OpMaker<OpSetIndexInternal<View>>::make(
    shared_ptr<typename OpSetIndexInternal<View>::SortedSet> sortedSet,
    UInt index) {
    return make_shared<OpSetIndexInternal<View>>(move(sortedSet), index);
}

#define opSetIndexInternalInstantiators(name)       \
    template struct OpSetIndexInternal<name##View>; \
    template struct OpMaker<OpSetIndexInternal<name##View>>;

buildForAllTypes(opSetIndexInternalInstantiators, );
#undef opSetIndexInternalInstantiators
