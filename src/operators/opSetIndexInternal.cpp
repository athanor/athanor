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
    if (setOperand->appearsDefined()) {
        getMember()->addTrigger(trigger, includeMembers, memberIndex);
    }
}

template <typename SetMemberViewType>
ExprRef<SetMemberViewType>& OpSetIndexInternal<SetMemberViewType>::getMember(
    size_t index) {
    auto& operandView = setOperand->view().checkedGet(
        "Error: OpSetIndex can't hanlde some cases where set becomes "
        "undefined.\n");
    auto& members = operandView.getMembers<SetMemberViewType>();
    debug_code(assert(index < parentSetMapping.size()));
    debug_code(assert(parentSetMapping[index] < members.size()));
    auto& member = members[parentSetMapping[index]];
    return member;
}

template <typename SetMemberViewType>
const ExprRef<SetMemberViewType>&
OpSetIndexInternal<SetMemberViewType>::getMember(size_t index) const {
    auto& operandView = setOperand->view().checkedGet(
        "Error: OpSetIndex can't hanlde some cases where set becomes "
        "undefined.\n");
    const auto& members = operandView.getMembers<SetMemberViewType>();
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
OptionalRef<SetMemberViewType> OpSetIndexInternal<SetMemberViewType>::view() {
    return getMember()->view();
}
template <typename SetMemberViewType>
OptionalRef<const SetMemberViewType>
OpSetIndexInternal<SetMemberViewType>::view() const {
    return getMember()->view();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::reevaluateDefined() {
    auto operandView = setOperand->getViewIfDefined();
    this->setAppearsDefined(operandView &&
                            parentSetMapping[index] <
                                operandView->numberElements() &&
                            getMember()->appearsDefined());
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::evaluateImpl() {
    setOperand->evaluate();

    evaluateMappings();
    reevaluateDefined();
}

template <typename SetMemberViewType>
void OpSetIndexInternal<SetMemberViewType>::evaluateMappings() {
    auto operandView = setOperand->getViewIfDefined();
    if (!operandView) {
        this->setAppearsDefined(false);
        parentSetMapping.clear();
        setParentMapping.clear();
        return;
    }

    parentSetMapping.resize(operandView->numberElements());
    iota(parentSetMapping.begin(), parentSetMapping.end(), 0);
    sort(parentSetMapping.begin(), parentSetMapping.end(),
         [&](size_t u, size_t v) {
             return smallerValue(getMember(u)->getViewIfDefined(),
                                 getMember(v)->getViewIfDefined());
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
    } else if (index == setOperand->view()->numberElements() - 1) {
        increment = -1;
    } else {
        increment = (smallerValue(getMember(index - 1)->view(),
                                  getMember(index)->view()))
                        ? 1
                        : -1;
    }
    size_t limit = (increment == 1) ? setOperand->view()->numberElements() : 0;
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
    parentSetMapping[index1] = parentSetMapping[index2];
    if (index1 == index) {
        notifyMemberSwapped();
    }

    parentSetMapping[index2] = temp;
    if (index2 == index) {
        notifyMemberSwapped();
    }

    swap(setParentMapping[parentSetMapping[index1]],
         setParentMapping[parentSetMapping[index2]]);
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

    void valueChanged() final {
        op->evaluateMappings();
        visitTriggers(
            [&](auto& t) {
                t->valueChanged();
                t->reattachTrigger();
            },
            op->triggers);
    }

    inline void memberValueChanged(UInt index, HashType) final {
        op->handleSetMemberValueChange(index);
    }

    inline void memberValuesChanged(const std::vector<UInt>& indices,
                                    const std::vector<HashType>&) final {
        for (auto index : indices) {
            op->handleSetMemberValueChange(index);
        }
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
void OpSetIndexInternal<SetMemberViewType>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    if (this->appearsDefined()) {
        getMember()->updateVarViolations(vioContext, vioContainer);
    } else {
        setOperand->updateVarViolations(vioContext, vioContainer);
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
pair<bool, ExprRef<SetMemberViewType>>
OpSetIndexInternal<SetMemberViewType>::optimise(PathExtension path) {
    auto optResult = setOperand->optimise(path.extend(setOperand));
    setOperand = optResult.second;
    return make_pair(optResult.first,
                     mpark::get<ExprRef<SetMemberViewType>>(path.expr));
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
    auto operandView = setOperand->getViewIfDefined();
    if (!operandView) {
        sanityCheck(!this->appearsDefined(),
                    "operator should be undefined, operand is undefined.");
        return;
    }
    sanityEqualsCheck(operandView->numberElements(), parentSetMapping.size());
    for (size_t i = 0; i < parentSetMapping.size() - 1; i++) {
        size_t u = i, v = i + 1;
        sanityCheck(smallerValue(getMember(u)->getViewIfDefined(),
                                 getMember(v)->getViewIfDefined()),
                    "Out of order mapping from parents to set");
    }
    sanityEqualsCheck(parentSetMapping.size(), setParentMapping.size());
    for (size_t i = 0; i < parentSetMapping.size(); i++) {
        sanityEqualsCheck(i, setParentMapping[parentSetMapping[i]]);
    }
    sanityEqualsCheck(parentSetMapping[index] < operandView->numberElements() &&
                          getMember()->appearsDefined(),
                      this->appearsDefined());
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
