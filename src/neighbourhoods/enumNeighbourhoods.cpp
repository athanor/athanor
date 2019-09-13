#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/enumVal.h"
#include "utils/random.h"

auto getRandomValueInDomain(const EnumDomain& d, StatsContainer& stats) {
    ++stats.minorNodeCount;
    return globalRandom<UInt>(0, d.numberValues() - 1);
}

template <>
void assignRandomValueInDomain<EnumDomain>(const EnumDomain& domain,
                                           EnumValue& val,
                                           StatsContainer& stats) {
    val.value = getRandomValueInDomain(domain, stats);
}

void enumAssignRandomGen(const EnumDomain& domain, int numberValsRequired,
                         std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "EnumAssignRandom", numberValsRequired,
        [&domain](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<EnumValue>().front());
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            auto backup = val.value;

            debug_neighbourhood_action(
                "Assigning random value: original value is " << asView(val));
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
        });
}

const NeighbourhoodVec<EnumDomain> NeighbourhoodGenList<EnumDomain>::value = {
    {1, enumAssignRandomGen}};

const NeighbourhoodVec<EnumDomain>
    NeighbourhoodGenList<EnumDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<EnumDomain>
    NeighbourhoodGenList<EnumDomain>::splitNeighbourhoods = {};
