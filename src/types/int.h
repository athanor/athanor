
#ifndef SRC_TYPES_INT_H_
#define SRC_TYPES_INT_H_
#include <utilities>
#include "common/common.h"
#include "types/domain.h"
#include "types/value.h"
struct IntDomain {
    std::vector<std::pair<int, int>> bounds;
};

struct IntValue {
    int64_t value;
    size_t
        containingBoundIndexCache;  // a cache of which bound this value lies in
    void assignInitialValueInDomain(IntDomain domain) {
        debug_code(assert(domain.bounds.size() > 0));
        containingBoundIndexCache = 0;
        value = domain.bounds[0].first;
    }
    template <typename Func>
    bool moveToNextValueInDomain(IntDomain domain, Func&& parentCheck) {
        while (true) {
            debug_code(
                assert(conta iningBoundIndexCache >= 0 &&
                       containingBoundIndexCache < domain.bounds.size()));
            if (value < domain.bounds[containingBoundIndexCache].second) {
                ++value;
            } else if (containingBoundIndexCache < domain.bounds.size() - 1) {
                ++containingBoundIndexCache;
                value = domain.bounds[containingBoundIndexCache].first;
                return true;
            } else {
                return false;
            }
            if (parentCheck(value)) {
                return true;
            }
        }
    }
};

#endif /* SRC_TYPES_INT_H_ */
