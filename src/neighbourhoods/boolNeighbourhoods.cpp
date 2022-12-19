#include <random>

#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/boolVal.h"
#include "utils/random.h"

UInt getRandomValueInDomain(const BoolDomain&) { return globalRandom(0, 1); }

template <>
bool assignRandomValueInDomain<BoolDomain>(
    const BoolDomain& domain, BoolValue& val,
    NeighbourhoodResourceTracker& resource) {
    if (!resource.requestResource()) {
        return false;
    }
    val.violation = getRandomValueInDomain(domain);
    return true;
}

void boolAssignRandomNeighbourhoodImpl(const BoolDomain& domain,
                                       NeighbourhoodParams& params) {
    auto& val = *(params.getVals<BoolValue>().front());
    int numberTries = 0;
    const int tryLimit = params.parentCheckTryLimit;
    auto backup = val.violation;

    debug_neighbourhood_action("Assigning random value: original value is "
                               << asView(val));
    bool success;
    do {
        success = val.changeValue([&]() {
            val.violation = getRandomValueInDomain(domain);
            ++params.stats.minorNodeCount;
            if (params.parentCheck(params.vals)) {
                return true;
            } else {
                val.violation = backup;
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
            val.violation = backup;
            return true;
        });
    }
}

void boolAssignRandomGen(const BoolDomain& domain, int numberValsRequired,
                         std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back("boolAssignRandom", numberValsRequired,
                                [&domain](NeighbourhoodParams& params) {
                                    boolAssignRandomNeighbourhoodImpl(domain,
                                                                      params);
                                });
}

Neighbourhood generateRandomReassignNeighbourhood(const BoolDomain& domain) {
    return Neighbourhood("boolAssignRandomDedicated", 1,
                         [&domain](NeighbourhoodParams& params) {
                             boolAssignRandomNeighbourhoodImpl(domain, params);
                         });
}

const NeighbourhoodVec<BoolDomain> NeighbourhoodGenList<BoolDomain>::value = {
    {1, boolAssignRandomGen}};

const NeighbourhoodVec<BoolDomain>
    NeighbourhoodGenList<BoolDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<BoolDomain>
    NeighbourhoodGenList<BoolDomain>::splitNeighbourhoods = {};
