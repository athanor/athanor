
#ifndef SRC_TYPES_INT_H_
#define SRC_TYPES_INT_H_
#include <utility>
#include <vector>

#include "forwardDecls/typesAndDomains.h"
struct IntDomain {
    std::vector<std::pair<int, int>> bounds;
};

struct IntValue {
    int64_t value;
    size_t containingBoundIndex;  // a cache of which bound this value lies in
};

#endif /* SRC_TYPES_INT_H_ */
