#include "base/exprRef.h"
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

template <typename View,
          typename Trigger = typename AssociatedTriggerType<View>::type>
shared_ptr<Trigger> fakeMakeTrigger(const ExprRef<View>&) {
    abort();
}

#define exprInstantiators(name)  name##Value name##Instantiator;
buildForAllTypes(exprInstantiators, );
#undef exprInstantiators
