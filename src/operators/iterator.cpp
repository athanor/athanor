#include "operators/iterator.h"
#include <iostream>
#include <memory>
#include "utils/ignoreUnused.h"

using namespace std;

template <typename View>
void Iterator<View>::addTriggerImpl(
    const shared_ptr<typename Iterator<View>::TriggerType>& trigger,
    bool includeMembers, Int memberIndex) {
    debug_code(assert(ref));
    auto otherTrigger = trigger;
    triggers.emplace_back(getTriggerBase(otherTrigger));
    ref->addTrigger(trigger, includeMembers, memberIndex);
}

template <typename View>
View& Iterator<View>::view() {
    debug_code(assert(ref));
    return ref->view();
    ;
}
template <typename View>
const View& Iterator<View>::view() const {
    debug_code(assert(ref));
    return ref->view();
}

template <typename View>
void Iterator<View>::evaluateImpl() {
    debug_code(assert(ref));
    ref->evaluate();
}

template <typename View>
Iterator<View>::Iterator(Iterator<View>&& other)
    : ExprInterface<View>(move(other)),
      triggers(move(other.triggers)),
      id(move(other.id)),
      ref(move(other.ref)) {}

template <typename View>
void Iterator<View>::startTriggeringImpl() {
    debug_code(assert(ref));
    ref->startTriggering();
}

template <typename View>
void Iterator<View>::stopTriggering() {
    debug_code(assert(ref));
    ref->stopTriggering();
}
template <typename View>
void Iterator<View>::updateVarViolationsImpl(const ViolationContext& vioContext,
                                             ViolationContainer& vioContainer) {
    debug_code(assert(ref));
    ref->updateVarViolations(vioContext, vioContainer);
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
    debug_code(assert(ref));
    return ref->isUndefined();
}

template <typename View>
pair<bool, ExprRef<View>> Iterator<View>::optimise(PathExtension path) {
    auto returnExpr = mpark::get<ExprRef<View>>(path.expr);
    if (!ref) {
        return make_pair(false, returnExpr);
    }

    auto result = ref->optimise(path.extend(ref));
    ref = result.second;
    return make_pair(result.first, returnExpr);
}

std::ostream& operator<<(std::ostream& os, const AnyIterRef& ref) {
    mpark::visit([&](auto& ref) { ref->dumpState(os); }, ref);
    return os;
}

#define iteratorInstantiators(name) template struct Iterator<name##View>;

buildForAllTypes(iteratorInstantiators, );
#undef iteratorInstantiators
