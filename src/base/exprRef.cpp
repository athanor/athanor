#include "base/exprRef.h"

#include <limits>
#include <memory>

#include "base/valRef.h"
#include "types/allVals.h"
#include "utils/ignoreUnused.h"
using namespace std;
bool sanityCheckRepeatMode = true;
bool hashCheckRepeatMode = true;
int TriggerDepthTracker::globalDepth = -1;
UInt64 triggerEventCount = 0;
UInt LARGE_VIOLATION = ((UInt)1) << ((sizeof(UInt) * 4) - 1);
UInt MAX_DOMAIN_SIZE = numeric_limits<UInt>().max();
BoolValue makeViolatingBoolValue() {
    BoolValue v;
    v.violation = LARGE_VIOLATION;
    return v;
}
BoolValue violatingBoolValue = makeViolatingBoolValue();
BoolView& VIOLATING_BOOL_VIEW = violatingBoolValue.view().get();

template <typename View>
OptionalRef<View> ExprInterface<View>::view() {
    return makeOptional(*static_cast<View*>(this));
}

template <typename View>
OptionalRef<const View> ExprInterface<View>::view() const {
    return makeOptional(*static_cast<const View*>(this));
}

template <typename View>
void ExprInterface<View>::addTriggerImpl(
    const std::shared_ptr<typename ExprInterface<View>::TriggerType>& trigger,
    bool includeMembers, Int memberIndex) {
    auto view = this->view();
    handleTriggerAdd(trigger, includeMembers, memberIndex, *view);
}

bool smallerValue(const AnyExprRef& u, const AnyExprRef& v) {
    return u.index() < v.index() ||
           (u.index() == v.index() &&
            lib::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = lib::get<BaseType<decltype(uImpl)>>(v);
                    return smallerValue(*uImpl->view(), *vImpl->view());
                },
                u));
}

bool largerValue(const AnyExprRef& u, const AnyExprRef& v) {
    return u.index() > v.index() ||
           (u.index() == v.index() &&
            lib::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = lib::get<BaseType<decltype(uImpl)>>(v);
                    return largerValue(*uImpl->view(), *vImpl->view());
                },
                u));
}

bool equalValue(const AnyExprRef& u, const AnyExprRef& v) {
    return u.index() == v.index() &&
           lib::visit(
               [&v](auto& uImpl) {
                   auto& vImpl = lib::get<BaseType<decltype(uImpl)>>(v);
                   return equalValue(*uImpl->getViewIfDefined(),
                                     *vImpl->getViewIfDefined());
               },
               u);
}

HashType getValueHash(const AnyExprRef& ref) {
    return lib::visit([&](auto& ref) { return getValueHash(*ref->view()); },
                      ref);
}

ostream& prettyPrint(ostream& os, const AnyExprRef& expr) {
    return lib::visit(
        [&](auto& ref) -> ostream& { return prettyPrint(os, ref->view()); },
        expr);
}

template <typename Value>
ExprRef<typename AssociatedViewType<Value>::type> ValRef<Value>::asExpr() {
    typedef typename AssociatedViewType<Value>::type View;
    return static_pointer_cast<View>(this->getPtr());
}
template <typename Value>
const ExprRef<typename AssociatedViewType<Value>::type> ValRef<Value>::asExpr()
    const {
    typedef typename AssociatedViewType<Value>::type View;
    return static_pointer_cast<View>(this->getPtr());
}

template <typename ViewType>
typename std::enable_if<
    IsViewType<ViewType>::value,
    const ValRef<typename AssociatedValueType<ViewType>::type>>::type
assumeAsValue(const ExprRef<ViewType>& viewPtr) {
    typedef typename AssociatedValueType<ViewType>::type ValueType;
    return static_pointer_cast<ValueType>(viewPtr.getPtr());
}

template <typename ViewType>
typename std::enable_if<
    IsViewType<ViewType>::value,
    ValRef<typename AssociatedValueType<ViewType>::type>>::type
assumeAsValue(ExprRef<ViewType>& viewPtr) {
    typedef typename AssociatedValueType<ViewType>::type ValueType;
    return static_pointer_cast<ValueType>(viewPtr.getPtr());
}

template <typename TriggerType>
const std::shared_ptr<TriggerBase> getTriggerBase(
    const std::shared_ptr<TriggerType>& trigger) {
    return static_pointer_cast<TriggerBase>(trigger);
}

#define exprInstantiators(name)                                                \
    template struct ExprInterface<name##View>;                                 \
    template struct ExprRef<name##View>;                                       \
    template struct ValRef<name##Value>;                                       \
    template ValRef<name##Value> assumeAsValue<name##View>(                    \
        ExprRef<name##View>&);                                                 \
    template const ValRef<name##Value> assumeAsValue<name##View>(              \
        const ExprRef<name##View>&);                                           \
    template const std::shared_ptr<TriggerBase> getTriggerBase<name##Trigger>( \
        const std::shared_ptr<name##Trigger>& trigger);                        \
    template <>                                                                \
    bool appearsDefined<name##View>(const name##View& view) {                  \
        return view.appearsDefined();                                          \
    }

buildForAllTypes(exprInstantiators, );
#undef exprInstantiators
