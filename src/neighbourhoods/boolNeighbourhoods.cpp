#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/bool.h"
#include "utils/random.h"

auto getRandomValueInDomain(const BoolDomain&) { return globalRandom(0, 1); }

template <>
void assignRandomValueInDomain<BoolDomain>(const BoolDomain& domain,
                                           BoolValue& val) {
    val.changeValue([&]() {
        val.violation = getRandomValueInDomain(domain);
        return true;
    });
}

void boolAssignRandomGen(const BoolDomain& domain,
                         std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "boolAssignRandom", [&domain](NeighbourhoodParams& params) {
            auto val = *mpark::get<ValRef<BoolValue>>(params.primary);
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            auto backup = val.violation;

            debug_neighbourhood_action(
                "Assigning random value: original value is " << asView(val));
            bool success;
            do {
                ++params.stats.minorNodeCount;
                success = val.changeValue([&]() {
                    val.violation = getRandomValueInDomain(domain);
                    return params.parentCheck(params.primary);
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
    boolAssignRandomGen};
