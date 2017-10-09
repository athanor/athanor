#include <cassert>
#include <random>
#include "types/int.h"
void assignRandomValueInDomain(const IntDomain& domain, IntValue& val) {
    u_int64_t randomDomainIndex = std::rand() % domain.domainSize;
    for (auto& bound : domain.bounds) {
        u_int64_t boundSize = (bound.second - bound.first) + 1;
        if (boundSize > randomDomainIndex) {
            val.value = bound.first + randomDomainIndex;
            return;
        } else {
            randomDomainIndex -= boundSize;
        }
    }
    assert(false);
    abort();
}
