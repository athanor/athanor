#include "operators/iterator.h"
#include <iostream>
#include <memory>
#include "utils/ignoreUnused.h"

using namespace std;

template <typename View>
void Iterator<View>::addTrigger(
    const shared_ptr<typename Iterator<View>::TriggerType>& trigger,
    bool includeMembers, Int memberIndex) {
    auto otherTrigger = trigger;
    triggers.emplace_back(getTriggerBase(otherTrigger));
    ref->addTrigger(trigger, includeMembers, memberIndex);
}

template <typename View>
View& Iterator<View>::view() {
    return ref->view();
    ;
}
template <typename View>
const View& Iterator<View>::view() const {
    return ref->view();
}

template <typename View>
void Iterator<View>::evaluateImpl() {
    ref->evaluate();
}

template <typename View>
Iterator<View>::Iterator(Iterator<View>&& other)
    : ExprInterface<View>(move(other)),
      triggers(move(other.triggers)),
      id(move(other.id)),
      ref(move(other.ref)) {}

template <typename View>
void Iterator<View>::startTriggering() {
    ref->startTriggering();
}

template <typename View>
void Iterator<View>::stopTriggering() {
    ref->stopTriggering();
}
template <typename View>
void Iterator<View>::updateVarViolations(const ViolationContext& vioContext,
                                         ViolationContainer& vioDesc) {
    ref->updateVarViolations(vioContext, vioDesc);
}

template <typename View>
ExprRef<View> Iterator<View>::deepCopySelfForUnrollImpl(
    const ExprRef<View>& self, const AnyIterRef& iterator) const {
    const auto iteratorPtr =
        mpark::get_if<IterRef<View>>(&iterator.asVariant());
    if (iteratorPtr != NULL && (**iteratorPtr).id == id) {
        return *iteratorPtr;
    } else {
        return self;
    }
}
template <typename View>
std::ostream& Iterator<View>::dumpState(std::ostream& os) const {
    os << "iter(";
    if (ref) {
        ref->dumpState(os);
    } else {
        os << "null";
    }
    os << ")";
    return os;
}

template <typename View>
void Iterator<View>::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    if (ref) {
        this->ref = findAndReplace(ref, func);
    }
}

template <typename View>
bool Iterator<View>::isUndefined() {
    return ref->isUndefined();
}

template <typename View>
bool Iterator<View>::optimise(PathExtension path) {
    return ref && ref->optimise(path.extend(ref));
}

std::ostream& operator<<(std::ostream& os, const AnyIterRef& ref) {
    mpark::visit([&](auto& ref) { ref->dumpState(os); }, ref);
    return os;
}

#define iteratorInstantiators(name) template struct Iterator<name##View>;

buildForAllTypes(iteratorInstantiators, );
#undef iteratorInstantiators
