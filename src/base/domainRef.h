
#ifndef SRC_BASE_DOMAINREF_H_
#define SRC_BASE_DOMAINREF_H_
#include "base/typeDecls.h"
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

#endif /* SRC_BASE_DOMAINREF_H_ */
