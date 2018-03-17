#include "base/exprRef.h"
u_int64_t getValueHash(const AnyExprRef& ref) {
    return mpark::visit([&](auto& ref) { return getValueHash(*ref); }, ref);
}
