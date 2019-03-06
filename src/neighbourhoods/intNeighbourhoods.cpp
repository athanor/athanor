#include <cassert>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"

#include "types/intVal.h"
#include "utils/random.h"

Int getRandomValueInDomain(const IntDomain& domain, StatsContainer& stats) {
    ++stats.minorNodeCount;
    UInt randomDomainIndex =
        globalRandom<decltype(domain.domainSize)>(0, domain.domainSize - 1);
    for (auto& bound : domain.bounds) {
        UInt boundSize = (bound.second - bound.first) + 1;
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
                                          IntValue& val,
                                          StatsContainer& stats) {
    val.changeValue([&]() {
        val.value = getRandomValueInDomain(domain, stats);
        return true;
    });
}

struct IntAssignRandom {
    const IntDomain& domain;
    IntAssignRandom(const IntDomain& domain) : domain(domain) {}

    void operator()(NeighbourhoodParams& params) {
        auto& val = *(params.getVals<IntValue>().front());
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Assigning random value: original value is "
                                   << asView(val));
        auto backup = val.value;
        bool success;
        do {
            success = val.changeValue([&]() {
                val.value = getRandomValueInDomain(domain, params.stats);
                if (params.parentCheck(params.vals)) {
                    return true;
                } else {
                    val.value = backup;
                    return false;
                }
            });
            if (success) {
                debug_neighbourhood_action("New value is " << asView(val));
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
    }
};

void intAssignRandomGen(const IntDomain& domain, int numberValsRequired,
                        std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back("intAssignRandom", numberValsRequired, IntAssignRandom(domain));
}

const NeighbourhoodVec<IntDomain> NeighbourhoodGenList<IntDomain>::value = {
    {1, intAssignRandomGen}};

const NeighbourhoodVec<IntDomain>
    NeighbourhoodGenList<IntDomain>::mergeNeighbourhoods = {};

const NeighbourhoodVec<IntDomain>
    NeighbourhoodGenList<IntDomain>::splitNeighbourhoods = {};
