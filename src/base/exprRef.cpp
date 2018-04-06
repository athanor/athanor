#include "base/exprRef.h"
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;
template <typename View>
void ExprInterface<View>::addTrigger(
    const std::shared_ptr<typename AssociatedTriggerType<View>::type>&
        trigger) {
    return this->view().triggers.emplace_back(trigger);
}
template <typename View>
View& ExprInterface<View>::view() {
    return *static_cast<View*>(this);
}
template <typename View>
const View& ExprInterface<View>::view() const {
    return *static_cast<const View*>(this);
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

void instantiateViewGetters(AnyExprRef& expr) {
    mpark::visit(
        [](auto& expr) {
            auto& view = expr->view();
            const auto& expr2 = expr;
            const auto& view2 = expr2->view();
            ignoreUnused(view, view2);
        },
        expr);
}

template <typename View,
          typename Trigger = typename AssociatedTriggerType<View>::type>
shared_ptr<Trigger> fakeMakeTrigger(const ExprRef<View>&) {
    abort();
}
void instantiate(AnyExprRef& expr) {
    mpark::visit(
        [&](auto& expr) {
            auto trigger = fakeMakeTrigger(expr);
            expr->addTrigger(trigger);
        },
        expr);
}
