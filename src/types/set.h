#ifndef SRC_TYPES_SET_H_
#define SRC_TYPES_SET_H_
#include <unordered_set>
#include <vector>
#include "utils/hashUtils.h"

#include "types/forwardDecls/typesAndDomains.h"

struct SetDomain {
    SizeAttr sizeAttr;
    Domain inner;
    // template hack to accept only domains
    template <
        typename DomainType,
        typename std::enable_if<IsDomainType<DomainType>::value, int>::type = 0>
    SetDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(std::make_shared<
                typename std::remove_reference<DomainType>::type>(
              std::forward<DomainType>(inner))) {}

    // template hack to accept only pointers to domains
    template <typename DomainPtrType,
              typename std::enable_if<IsDomainPtrType<DomainPtrType>::value,
                                      int>::type = 0>
    SetDomain(SizeAttr sizeAttr, DomainPtrType&& inner)
        : sizeAttr(sizeAttr), inner(std::forward<DomainPtrType>(inner)) {}
};
template <typename Inner>
struct SetValueImpl {
    std::vector<Inner> members;
    std::unordered_set<u_int64_t> memberHashes;
    u_int64_t cachedHashTotal = 0;
    inline bool containsMember(const Inner& member) {
        return memberHashes.count(mix(getHash(*member)));
    }
};
#define variantValues(T) SetValueImpl<std::shared_ptr<T##Value>>
typedef mpark::variant<buildForAllTypes(variantValues, MACRO_COMMA)>
    SetValueImplWrapper;
#undef variantValues

struct SetValue : public DirtyFlag {
    SetValueImplWrapper setValueImpl;
};

#endif /* SRC_TYPES_SET_H_ */
