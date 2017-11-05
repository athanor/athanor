#include <cassert>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/int.h"
#include "utils/random.h"
void assignRandomValueInDomain(const IntDomain& domain, IntValue& val) {
    u_int64_t randomDomainIndex =
        globalRandom<decltype(domain.domainSize)>(0, domain.domainSize - 1);
    for (auto& bound : domain.bounds) {
        u_int64_t boundSize = (bound.second - bound.first) + 1;
        if (boundSize > randomDomainIndex) {
            val.changeValue(
                [&]() { val.value = bound.first + randomDomainIndex; });
            return;
        } else {
            randomDomainIndex -= boundSize;
        }
    }
    assert(false);
    abort();
}

const NeighbourhoodGenList<IntDomain>::type
    NeighbourhoodGenList<IntDomain>::value = {};
