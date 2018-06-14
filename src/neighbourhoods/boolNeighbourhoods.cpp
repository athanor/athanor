#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/bool.h"
#include "utils/random.h"

auto getRandomValueInDomain(const BoolDomain&, StatsContainer& stats) {
    ++stats.minorNodeCount;
    return globalRandom(0, 1);
}

template <>
void assignRandomValueInDomain<BoolDomain>(const BoolDomain& domain,
                                           BoolValue& val,
                                           StatsContainer& stats) {
    val.changeValue([&]() {
        val.violation = getRandomValueInDomain(domain, stats);
        return true;
    });
}

void boolAssignRandomGen(const BoolDomain& domain, int numberValsRequired,
                         std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "boolAssignRandom", numberValsRequired,
        [&domain](NeighbourhoodParams& params) {
            auto val = *(params.getVals<BoolValue>().front());
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            auto backup = val.violation;

            debug_neighbourhood_action(
                "Assigning random value: original value is " << asView(val));
            bool success;
            do {
                success = val.changeValue([&]() {
                    val.violation =
                        getRandomValueInDomain(domain, params.stats);
                    return params.parentCheck(params.vals);
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
        });
}

const NeighbourhoodVec<BoolDomain> NeighbourhoodGenList<BoolDomain>::value = {
    {1, boolAssignRandomGen}};
