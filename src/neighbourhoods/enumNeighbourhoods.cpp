#include <random>

#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/enumVal.h"
#include "utils/random.h"

auto getRandomValueInDomain(const EnumDomain& d) {
    return globalRandom<UInt>(0, d.numberValues() - 1);
}

template <>
bool assignRandomValueInDomain<EnumDomain>(
    const EnumDomain& domain, EnumValue& val,
    NeighbourhoodResourceTracker& resource) {
    if (!resource.requestResource()) {
        return false;
    }
    val.value = getRandomValueInDomain(domain);
    return true;
}

void enumAssignRandomNeighbourhoodImpl(const EnumDomain& domain,
                                       NeighbourhoodParams& params) {
    auto& val = *(params.getVals<EnumValue>().front());
    int numberTries = 0;
    const int tryLimit = params.parentCheckTryLimit;
    auto backup = val.value;

    debug_neighbourhood_action("Assigning random value: original value is "
                               << asView(val));
    bool success;
    do {
        success = val.changeValue([&]() {
            UInt old_value = val.value;
            Int counter = 50;
            while(val.value == old_value && --counter) {
                val.value = getRandomValueInDomain(domain);
            }
            ++params.stats.minorNodeCount;
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

void enumAssignRandomGen(const EnumDomain& domain, int numberValsRequired,
                         std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back("EnumAssignRandom", numberValsRequired,
                                [&domain](NeighbourhoodParams& params) {
                                    enumAssignRandomNeighbourhoodImpl(domain,
                                                                      params);
                                });
}

Neighbourhood generateRandomReassignNeighbourhood(const EnumDomain& domain) {
    return Neighbourhood("EnumAssignRandomDedicated", 1,
                         [&domain](NeighbourhoodParams& params) {
                             enumAssignRandomNeighbourhoodImpl(domain, params);
                         });
}

const NeighbourhoodVec<EnumDomain> NeighbourhoodGenList<EnumDomain>::value = {
    {1, enumAssignRandomGen}};

const NeighbourhoodVec<EnumDomain>
    NeighbourhoodGenList<EnumDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<EnumDomain>
    NeighbourhoodGenList<EnumDomain>::splitNeighbourhoods = {};
