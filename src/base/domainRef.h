
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
inline u_int64_t getDomainSize(const AnyDomainRef& domain, T = 0) {
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

#endif /* SRC_BASE_DOMAINREF_H_ */
