#include "base/exprRef.h"
#include <memory>
#include "base/valRef.h"
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename View>
View& ExprInterface<View>::view() {
    return *static_cast<View*>(this);
}
template <typename View>
const View& ExprInterface<View>::view() const {
    return *static_cast<const View*>(this);
}

template <typename View>
void ExprInterface<View>::addTrigger(
    const std::shared_ptr<typename ExprInterface<View>::TriggerType>& trigger) {
    this->view().triggers.emplace_back(trigger);
}

HashType getValueHash(const AnyExprRef& ref) {
    return mpark::visit([&](auto& ref) { return getValueHash(ref->view()); },
                        ref);
}

ostream& prettyPrint(ostream& os, const AnyExprRef& expr) {
    return mpark::visit(
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
        const std::shared_ptr<name##Trigger>& trigger);

buildForAllTypes(exprInstantiators, );
#undef exprInstantiators
