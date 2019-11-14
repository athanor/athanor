#include "base/domainRef.h"
#include "types/allVals.h"
#include "utils/ignoreUnused.h"
using namespace std;

AnyDomainRef deepCopyDomain(AnyDomainRef domain) {
    return lib::visit(
        [&](auto& domain) -> AnyDomainRef { return domain->deepCopy(domain); },
        domain);
}

size_t getResourceLowerBound(const AnyDomainRef& domain) {
    return lib::visit(
        [&](auto& domain) {
            debug_code(assert(domain));
            return getResourceLowerBound(*domain);
        },
        domain);
}
