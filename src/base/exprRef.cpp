#include "base/exprRef.h"
#include <memory>
#include "base/valRef.h"
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;
u_int64_t triggerEventCount = 0;
UInt LARGE_VIOLATION = ((UInt)1) << ((sizeof(UInt) * 4) - 1);

template <typename View>
View& ExprInterface<View>::view() {
    return *static_cast<View*>(this);
}
template <typename View>
const View& ExprInterface<View>::view() const {
    return *static_cast<const View*>(this);
}

template <typename View, typename Trigger>
auto addAsMemberTrigger(View& view, const std::shared_ptr<Trigger>& trigger,
                        Int memberIndex)
    -> decltype(view.allMemberTriggers, void()) {
    if (memberIndex == -1) {
        view.allMemberTriggers.emplace_back(trigger);
    } else {
        if ((Int)view.singleMemberTriggers.size() <= memberIndex) {
            view.singleMemberTriggers.resize(memberIndex + 1);
        }
        view.singleMemberTriggers[memberIndex].emplace_back(trigger);
    }
}

template <typename... Args>
auto addAsMemberTrigger(Args&&...) {}

template <typename View>
void ExprInterface<View>::addTrigger(
    const std::shared_ptr<typename ExprInterface<View>::TriggerType>& trigger,
    bool includeMembers, Int memberIndex) {
    this->view().triggers.emplace_back(trigger);
    if (includeMembers) {
        addAsMemberTrigger(this->view(), trigger, memberIndex);
    }
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
