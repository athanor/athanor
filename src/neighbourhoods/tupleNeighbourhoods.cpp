#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/tupleVal.h"
#include "utils/random.h"

using namespace std;

template <>
void assignRandomValueInDomain<TupleDomain>(const TupleDomain& domain,
                                            TupleValue& val,
                                            StatsContainer& stats) {
    // populate tuple with unassigned members if it is empty

    while (val.members.size() < domain.inners.size()) {
        mpark::visit(
            [&](auto& innerDomainPtr) {
                auto newMember = constructValueFromDomain(*innerDomainPtr);
                val.addMember(newMember);
            },
            domain.inners[val.members.size()]);
    }
    for (size_t i = 0; i < domain.inners.size(); i++) {
        mpark::visit(
            [&](const auto& innerDomainPtr) {
                typedef
                    typename BaseType<decltype(innerDomainPtr)>::element_type
                        Domain;
                typedef typename AssociatedValueType<Domain>::type Value;
                auto& memberVal = *val.member<Value>(i);
                assignRandomValueInDomain(*innerDomainPtr, memberVal, stats);
            },
            domain.inners[i]);
    }
}

void tupleAssignRandomGen(const TupleDomain& domain, int numberValsRequired,
                          std::vector<Neighbourhood>& neighbourhoods) {
    for (auto& inner : domain.inners) {
        if (!mpark::get_if<shared_ptr<IntDomain>>(&inner) &&
            !mpark::get_if<shared_ptr<BoolDomain>>(&inner)) {
            return;
        }
    }
    neighbourhoods.emplace_back(
        "TupleAssignRandom", numberValsRequired,
        [&domain](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<TupleValue>().front());
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            debug_neighbourhood_action(
                "Assigning random value: original value is " << asView(val));
            auto backup = deepCopy(val);
            backup->container = val.container;
            auto newValue = constructValueFromDomain(domain);
            newValue->container = val.container;
            bool success;
            do {
                assignRandomValueInDomain(domain, *newValue, params.stats);
                success = val.tryAssignNewValue(*newValue, [&]() {
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
                deepCopy(*backup, val);
            }
        });
}

const NeighbourhoodVec<TupleDomain> NeighbourhoodGenList<TupleDomain>::value = {
    {1, tupleAssignRandomGen}};

const NeighbourhoodVec<TupleDomain>
    NeighbourhoodGenList<TupleDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<TupleDomain>
    NeighbourhoodGenList<TupleDomain>::splitNeighbourhoods = {};
