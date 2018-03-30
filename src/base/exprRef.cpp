#include "base/exprRef.h"
HashType getValueHash(const AnyExprRef& ref) {
    return mpark::visit([&](auto& ref) { return getValueHash(*ref); }, ref);
}

AnyExprRef deepCopyForUnroll(const AnyExprRef& expr,
                             const AnyIterRef& iterator) {
    return mmpark::visit(
        [&](auto& expr) -> AnyExprRef {
            return deepCopyForUnroll(expr, iterator);
        },
        expr);
}
