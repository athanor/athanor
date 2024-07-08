#include <cassert>
#include <random>

#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "search/ucbSelector.h"
#include "types/intVal.h"
#include "utils/random.h"
using namespace std;
Int getRandomValueInDomain(const IntDomain& domain);
Int getRandomValueInDomain(const IntDomain& domain) {
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
    myCerr << "Error: empty  domain.\n";
    signalEndOfSearch();
    abort();  // put here just to stop warning
}

Int chooseBound(const pair<Int, Int>& bound1, const pair<Int, Int>& bound2,
                Int newValue) {
    Int distanceToBound1 = newValue - bound1.second;
    Int distanceToBound2 = bound2.first - newValue;
    if (distanceToBound1 < distanceToBound2) {
        return bound1.second;
    } else if (distanceToBound2 < distanceToBound1) {
        return bound2.first;
    } else {
        return (globalRandom<int>(0, 1)) ? bound1.second : bound2.first;
    }
}

Int getRandomValueInDomain(const IntDomain& domain, Int minValue,
                           Int maxValue) {
    if (domain.bounds.empty()) {
        myCerr << "Error: empty domain\n";
        signalEndOfSearch();
    }
    minValue = max(minValue, domain.bounds.front().first);
    maxValue = min(maxValue, domain.bounds.back().second);

    Int newValue = globalRandom<Int>(minValue, maxValue);
    // check that number is in domain
    // if not choose upper of previous bound, lower of next, which ever is
    // closer.
    // binary search is likely most efficient here, just using simpler
    // loop for now
    for (size_t i = 0; i < domain.bounds.size(); i++) {
        auto& bound = domain.bounds[i];
        if (bound.first <= newValue && newValue <= bound.second) {
            return newValue;
        }
        if (i + 1 < domain.bounds.size() &&
            newValue < domain.bounds[i + 1].first) {
            return chooseBound(bound, domain.bounds[i + 1], newValue);
        }
    }
    cout << domain << endl;
    cout << minValue << endl;
    cout << maxValue << endl;
    assert(false);
    myAbort();
}

template <>
bool assignRandomValueInDomain<IntDomain>(
    const IntDomain& domain, IntValue& val,
    NeighbourhoodResourceTracker& resource) {
    if (!val.domain) {
        val.domain = &domain;
    }
    if (!resource.requestResource()) {
        return false;
    }
    val.value = getRandomValueInDomain(domain);
    return true;
}
struct IntAssignRandom {
    const IntDomain& domain;
    IntAssignRandom(const IntDomain& domain) : domain(domain) {}

    void operator()(NeighbourhoodParams& params) {
        auto& val = *(params.getVals<IntValue>().front());
        int numberTries = 00;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Assigning random value: original value is "
                                   << asView(val));
        auto backup = val.value;
        UInt varViolation = params.vioContainer.varViolation(val.id);
        bool success;
        do {
            ++params.stats.minorNodeCount;
            success = val.changeValue([&]() {
                    Int old_value = val.value;
                    Int counter = 50;
                    while(val.value == old_value && --counter) {
                        val.value = getRandomValueInDomain(domain);
                    }
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
        } while (!success && tryLimitCheck(++numberTries, tryLimit));
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find value, number tries=" << tryLimit);
            return;
        }

        if (params.changeAccepted()) {

        } else {
            debug_neighbourhood_action("Change rejected");
            val.changeValue([&]() {
                val.value = backup;
                return true;
            });
        }
    }
};

struct IntAssignRandomViolation {
    const IntDomain& domain;
    IntAssignRandomViolation(const IntDomain& domain) : domain(domain) {}

    void operator()(NeighbourhoodParams& params) {
        auto& val = *(params.getVals<IntValue>().front());
        int numberTries = 00;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Assigning random value with violations: original value is "
                                   << asView(val));
        auto backup = val.value;
        UInt varViolation = params.vioContainer.varViolation(val.id);
        bool success;
        do {
            ++params.stats.minorNodeCount;
            success = val.changeValue([&]() {
                if (varViolation == 0) {
                    Int old_value = val.value;
                    Int counter = 50;
                    while(val.value == old_value && --counter) {
                        val.value = getRandomValueInDomain(domain);
                    }
                } else {
                    Int old_value = val.value;
                    Int counter = 50;
                    while(val.value == old_value && --counter) {
                        val.value =
                            getRandomValueInDomain(domain, val.value - varViolation,
                                                   val.value + varViolation);
                    }
                }
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
        } while (!success && tryLimitCheck(++numberTries, tryLimit));
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find value, number tries=" << tryLimit);
            return;
        }

        if (params.changeAccepted()) {

        } else {

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
    neighbourhoods.emplace_back("intAssignRandom", numberValsRequired,
                                IntAssignRandom(domain));
}

void intAssignRandomGenViolation(const IntDomain& domain, int numberValsRequired,
                        std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back("intAssignRandomViolation", numberValsRequired,
                                IntAssignRandomViolation(domain));
}

Neighbourhood generateRandomReassignNeighbourhood(const IntDomain& domain) {
    return Neighbourhood("intAssignRandomDedicated", 1,
                         IntAssignRandom(domain));
}

const NeighbourhoodVec<IntDomain> NeighbourhoodGenList<IntDomain>::value = {
    {1, intAssignRandomGen},
    {1, intAssignRandomGenViolation}};

const NeighbourhoodVec<IntDomain>
    NeighbourhoodGenList<IntDomain>::mergeNeighbourhoods = {};

const NeighbourhoodVec<IntDomain>
    NeighbourhoodGenList<IntDomain>::splitNeighbourhoods = {};
