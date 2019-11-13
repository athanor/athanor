
#ifndef SRC_BASE_DOMAINREF_H_
#define SRC_BASE_DOMAINREF_H_
#include "base/typeDecls.h"
#include "base/valRef.h"
#include "common/common.h"
#include "utils/variantOperations.h"

#define variantDomains(T) std::shared_ptr<T##Domain>
using AnyDomainRef =
    mpark::variant<buildForAllTypes(variantDomains, MACRO_COMMA)>;
#undef variantDomains

template <typename DomainType>
ValRef<typename AssociatedValueType<DomainType>::type> constructValueFromDomain(
    const DomainType& domain) {
    auto val = make<typename AssociatedValueType<DomainType>::type>();
    matchInnerType(domain, *val);
    return val;
}

template <typename T = int>
inline UInt getDomainSize(const AnyDomainRef& domain, T = 0) {
    return mpark::visit(
        [&](const auto& domainImpl) { return getDomainSize(*domainImpl); },
        domain);
}

template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyDomainRef& d,
                                 T = 0) {
    return mpark::visit(
        [&os](auto& dImpl) -> std::ostream& { return prettyPrint(os, *dImpl); },
        d);
}

inline std::ostream& operator<<(std::ostream& os, const AnyDomainRef& d) {
    return prettyPrint(os, d);
}
// template hack to accept only domains
template <typename DomainType,
          typename std::enable_if<IsDomainType<BaseType<DomainType>>::value,
                                  int>::type = 0>
AnyDomainRef makeAnyDomainRef(DomainType&& d) {
    return std::make_shared<typename std::remove_reference<DomainType>::type>(
        std::forward<DomainType>(d));
}

// template hack to accept only pointers to domains
template <typename DomainPtrType,
          typename std::enable_if<
              IsDomainPtrType<BaseType<DomainPtrType>>::value, int>::type = 0>
AnyDomainRef makeAnyDomainRef(DomainPtrType&& d) {
    return std::forward<DomainPtrType>(d);
}

template <typename AnyDomainRefType,
          typename std::enable_if<
              std::is_same<BaseType<AnyDomainRefType>, AnyDomainRef>::value,
              int>::type = 0>
decltype(auto) makeAnyDomainRef(AnyDomainRefType&& d) {
    return std::forward<AnyDomainRefType>(d);
}
void mergeDomains(AnyDomainRef& dest, AnyDomainRef& src);
AnyDomainRef deepCopyDomain(AnyDomainRef other);

template <typename Domain>
typename std::enable_if<IsDomainType<Domain>::value, size_t>::type
getNumberElementsLowerBound(const Domain& domain);

size_t getNumberElementsLowerBound(const AnyDomainRef&);

#endif /* SRC_BASE_DOMAINREF_H_ */
