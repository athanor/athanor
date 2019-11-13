#include "base/domainRef.h"
#include "types/allVals.h"
#include "utils/ignoreUnused.h"
using namespace std;

AnyDomainRef deepCopyDomain(AnyDomainRef domain) {
    return mpark::visit(
        [&](auto& domain) -> AnyDomainRef { return domain->deepCopy(domain); },
        domain);
}

size_t getNumberElementsLowerBound(const AnyDomainRef& domain) {
    return mpark::visit(
        [&](auto& domain) {
            debug_code(assert(domain));
            return getNumberElementsLowerBound(*domain);
        },
        domain);
}
