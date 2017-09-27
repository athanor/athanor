#include "types/int.h"
#include "common/common.h"
#include "types/parentCheck.h"
void assignInitialValueInDomain(IntValue& val, const IntDomain& domain) {
    debug_code(assert(domain.bounds.size() > 0));
    val.containingBoundIndex = 0;
    val.value = domain.bounds[0].first;
}

bool moveToNextValueInDomain(IntValue& val, const IntDomain& domain,
                             const ParentCheck& parentCheck) {
    while (true) {
        debug_code(assert(val.containingBoundIndex >= 0 &&
                          val.containingBoundIndex < domain.bounds.size()));
        if (val.value < domain.bounds[val.containingBoundIndex].second) {
            ++val.value;
        } else if (val.containingBoundIndex < domain.bounds.size() - 1) {
            ++val.containingBoundIndex;
            val.value = domain.bounds[val.containingBoundIndex].first;
        } else {
            return false;
        }
        if (parentCheck()) {
            return true;
        }
    }
}

u_int64_t getHash(const IntValue& val) { return val.value; }
