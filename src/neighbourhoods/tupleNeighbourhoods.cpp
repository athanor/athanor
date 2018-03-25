#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/tuple.h"
#include "utils/random.h"

using namespace std;
static ViolationDescription emptyViolations;

template <>
void assignRandomValueInDomain<TupleDomain>(const TupleDomain& domain,
                                            TupleValue& val) {
    // clear tuple and populate with new random elements
    val.members.clear();

    for (size_t i = 0; i < domain.inners.size(); i++) {
        mpark::visit(
            [&](auto& innerDomainPtr) {
                auto newMember = constructValueFromDomain(*innerDomainPtr);
                assignRandomValueInDomain(*innerDomainPtr, *newMember);
                val.addMember(newMember);
            },
            domain.inners[i]);
    }
}

const NeighbourhoodVec<TupleDomain> NeighbourhoodGenList<TupleDomain>::value =
    {};
