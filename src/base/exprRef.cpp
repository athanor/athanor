#include "base/exprRef.h"
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename View>
View& ExprInterface::view() {
    return *static_cast<View*>(this);
}
template <typename View>
const View& ExprInterface::view() const {
    return *static_cast<View*>(this);
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
    mpark::visit([](auto& expr) {
        auto& view = expr->view();
        const auto& expr2 = expr;
        const auto& view2 = expr2->view();
        ignoreUnused(expr, expr2);
    } expr)
}
