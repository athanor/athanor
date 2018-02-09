#include <cassert>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/int.h"
#include "utils/random.h"

int64_t getRandomValueInDomain(const IntDomain& domain) {
    u_int64_t randomDomainIndex =
        globalRandom<decltype(domain.domainSize)>(0, domain.domainSize - 1);
    for (auto& bound : domain.bounds) {
        u_int64_t boundSize = (bound.second - bound.first) + 1;
        if (boundSize > randomDomainIndex) {
            return bound.first + randomDomainIndex;
        } else {
            randomDomainIndex -= boundSize;
        }
    }
    assert(false);
    abort();
}

template <>
void assignRandomValueInDomain<IntDomain>(const IntDomain& domain,
                                          IntValue& val) {
    val.changeValue([&]() {
        val.value = getRandomValueInDomain(domain);
        return true;
    });
}

void intAssignRandomGen(const IntDomain& domain,
                        std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "intAssignRandom", [&domain](NeighbourhoodParams& params) {
            auto& val = *mpark::get<ValRef<IntValue>>(params.primary);
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            debug_neighbourhood_action(
                "Assigning random value: original value is " << val);
            auto backup = val.value;
            bool success;
            do {
                ++params.stats.minorNodeCount;
                success = val.changeValue([&]() {
                    val.value = getRandomValueInDomain(domain);
                    if (params.parentCheck(params.primary)) {
                        return true;
                    } else {
                        val.value = backup;
                        return false;
                    }
                });
                if (success) {
                    debug_neighbourhood_action("New value is " << val);
                }
            } while (!success && ++numberTries < tryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find value, number tries=" << tryLimit);
                return;
            }
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                val.changeValue([&]() {
                    val.value = backup;
                    return true;
                });
            }
        });
}

const NeighbourhoodGenList<IntDomain>::type
    NeighbourhoodGenList<IntDomain>::value = {intAssignRandomGen};
