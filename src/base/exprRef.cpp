#include "base/exprRef.h"
#include "types/allTypes.h"
using namespace std;

HashType getValueHash(const AnyExprRef& ref) {
    return mpark::visit([&](auto& ref) { return getValueHash(*ref); }, ref);
}

ostream& prettyPrint(ostream& os, const AnyExprRef& expr) {
    return mpark::visit(
        [&](auto& ref) -> ostream& { return prettyPrint(os, *ref); }, expr);
}

AnyExprRef deepCopyForUnroll(const AnyExprRef& expr,
                             const AnyIterRef& iterator) {
    return mpark::visit(
        [&](auto& expr) -> AnyExprRef {
            return deepCopyForUnroll(expr, iterator);
        },
        expr);
}
