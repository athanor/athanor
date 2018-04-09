#include "operators/iterator.h"
#include <iostream>
#include <memory>
#include "utils/ignoreUnused.h"

using namespace std;

#define iteratorInstantiators(name) \
    template <>                     \
    struct Iterator<name##View>;    \
    Iterator<name##View> name##IteratorInstantiator(0, nullptr);
buildForAllTypes(iteratorInstantiators, );
#undef iteratorInstantiators

template <typename View>
void Iterator<View>::addTrigger(
    const shared_ptr<typename Iterator<View>::TriggerType>& trigger) {
    triggers.emplace_back(trigger);
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
void Iterator<View>::evaluate() {
    ref->evaluate();
}

template <typename View>
Iterator<View>::Iterator(Iterator<View>&& other)
    : ExprInterface<View>(move(other)),
      triggers(move(other.triggers)),
      id(move(other.id)),
      ref(move(other.ref)),
      refTrigger(move(other.refTrigger)) {
    refTrigger->RecipientTriggers = &triggers;
}

template <typename View>
void Iterator<View>::startTriggering() {
    if (!refTrigger) {
        auto trigger = makeForwardingTrigger<TriggerType>(&triggers);
        refTrigger = getForwardingTriggerBase(trigger);
        ref->addTrigger(getAsTriggerType(trigger));
        ref->startTriggering();
    }
}

template <typename View>
void Iterator<View>::stopTriggering() {
    if (refTrigger) {
        deleteTrigger(refTrigger);
        refTrigger = nullptr;
        ref->stopTriggering();
    }
}
template <typename View>
void Iterator<View>::updateViolationDescription(UInt parentViolation,
                                                ViolationDescription& vioDesc) {
    ref->updateViolationDescription(parentViolation, vioDesc);
}

template <typename View>
ExprRef<View> Iterator<View>::deepCopySelfForUnroll(
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
    ref->dumpState(os);
    os << ")";
    return os;
}

template <typename View>
void Iterator<View>::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    if (ref) {
        this->ref = findAndReplace(ref, func);
    }
}
