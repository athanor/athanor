#include "operators/iterator.h"
#include <iostream>
#include <memory>
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"

using namespace std;

template <typename View>
void Iterator<View>::addTriggerImpl(
    const shared_ptr<typename Iterator<View>::TriggerType>& trigger,
    bool includeMembers, Int memberIndex) {
    debug_code(assert(ref));
    triggers.emplace_back(getTriggerBase(trigger));
    trigger->flags.template get<TriggerBase::ALLOW_DEFINEDNESS_TRIGGERS>() =
        false;
    ref->addTrigger(trigger, includeMembers, memberIndex);
}

template <typename View>
OptionalRef<View> Iterator<View>::view() {
    debug_code(assert(ref));
    return ref->view();
}
template <typename View>
OptionalRef<const View> Iterator<View>::view() const {
    debug_code(assert(ref));
    return ref->view();
}

template <typename View>
struct Iterator<View>::RefTrigger
    : public ChangeTriggerAdapter<typename Iterator<View>::TriggerType,
                                  typename Iterator<View>::RefTrigger> {
    Iterator<View>* op;
    RefTrigger(Iterator<View>* op) : op(op) {
        this->flags
            .template get<TriggerBase::ALLOW_ONLY_DEFINEDNESS_TRIGGERS>() =
            true;
    }
    void adapterValueChanged() {}
    void adapterHasBecomeUndefined() {
        this->op->setAppearsDefined(false);
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); }, op->triggers);
    }
    void adapterHasBecomeDefined() {
        this->op->setAppearsDefined(true);
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers);
    }
    ExprRef<View>& getTriggeringOperand() { return op->ref; }
    void reattachTrigger() {
        deleteTrigger(op->refTrigger);
        auto trigger = make_shared<typename Iterator<View>::RefTrigger>(op);
        op->ref->addTrigger(trigger);
        op->refTrigger = trigger;
    }
};
template <typename View>
void Iterator<View>::evaluateImpl() {
    debug_code(assert(ref));
    ref->evaluate();
    this->setAppearsDefined(ref->appearsDefined());
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
    if (!refTrigger) {
        refTrigger = make_shared<RefTrigger>(this);
        ref->addTrigger(refTrigger);
        ref->startTriggering();
    }
}

template <typename View>
void Iterator<View>::stopTriggeringOnChildren() {
    if (refTrigger) {
        deleteTrigger(refTrigger);
        refTrigger = nullptr;
    }
}

template <typename View>
void Iterator<View>::stopTriggering() {
    debug_code(assert(ref));
    if (refTrigger) {
        stopTriggeringOnChildren();
        ref->stopTriggering();
    }
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

template <typename View>
string Iterator<View>::getOpName() {
    return "Iterator<" +
           TypeAsString<typename AssociatedValueType<View>::type>::value + ">";
}
template <typename View>
void Iterator<View>::debugSanityChecksImpl() {
    if (!ref) {
        return;
    }
    ref->debugSanityCheck();
    sanityCheck(
        ref->appearsDefined() == this->appearsDefined(),
        toString("ref and iterator definedness do not match: ref defined=",
                 ref->appearsDefined(),
                 ", iterator defined=", this->appearsDefined()));
}

#define iteratorInstantiators(name) template struct Iterator<name##View>;

buildForAllTypes(iteratorInstantiators, );
#undef iteratorInstantiators
