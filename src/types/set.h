#ifndef SRC_TYPES_SET_H_
#define SRC_TYPES_SET_H_
#include <unordered_set>
#include <vector>

#include "types/forwardDecls/typesAndDomains.h"

struct SetDomain {
    SizeAttr sizeAttr;
    Domain inner;
    SetDomain() : sizeAttr(noSize()) {}
};
template <typename Inner>
struct SetValueImpl {
    std::vector<Inner> members;
    std::unordered_set<u_int64_t> memberHashes;
    u_int64_t cachedHashTotal;
};

struct SetValue {
#define variantValues(T) SetValueImpl<std::shared_ptr<T##Value>>
    mpark::variant<buildForAllTypes(variantValues, MACRO_COMMA)> setValueImpl;
#undef variantValues
};

#endif /* SRC_TYPES_SET_H_ */
