
#ifndef SRC_TYPES_FORWARDDECLS_GETDOMAINSIZE_H_
#define SRC_TYPES_FORWARDDECLS_GETDOMAINSIZE_H_
#include "types/base.h"
#define makeGetDomainSizeDecl(name) \
    u_int64_t getDomainSize(const name##Domain&);
buildForAllTypes(makeGetDomainSizeDecl, )
#undef makeGetDomainSizeDecl

    inline u_int64_t getDomainSize(const Domain& domain) {
    return mpark::visit(
        [&](const auto& domainImpl) { return getDomainSize(*domainImpl); },
        domain);
}

#endif /* SRC_TYPES_FORWARDDECLS_GETDOMAINSIZE_H_ */
