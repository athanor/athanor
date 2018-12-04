#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/tuple.h"
#include "utils/random.h"

using namespace std;

template <>
void assignRandomValueInDomain<TupleDomain>(const TupleDomain& domain,
                                            TupleValue& val,
                                            StatsContainer& stats) {
    // clear tuple and populate with new random elements
    val.members.clear();

    for (size_t i = 0; i < domain.inners.size(); i++) {
        mpark::visit(
            [&](auto& innerDomainPtr) {
                auto newMember = constructValueFromDomain(*innerDomainPtr);
                assignRandomValueInDomain(*innerDomainPtr, *newMember, stats);
                val.addMember(newMember);
            },
            domain.inners[i]);
    }
}

const NeighbourhoodVec<TupleDomain> NeighbourhoodGenList<TupleDomain>::value =
    {};
