#include "base/domainRef.h"
#include "types/allVals.h"
#include "utils/ignoreUnused.h"
using namespace std;

AnyDomainRef deepCopyDomain(AnyDomainRef domain) {
    return mpark::visit(
        [&](auto& domain) -> AnyDomainRef { return domain->deepCopy(domain); },
        domain);
}
