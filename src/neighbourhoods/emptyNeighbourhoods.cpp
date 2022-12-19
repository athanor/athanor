#include <cmath>
#include <random>

#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/emptyVal.h"
#include "utils/random.h"

using namespace std;

template <>
bool assignRandomValueInDomain<EmptyDomain>(const EmptyDomain&, EmptyValue&,
                                            NeighbourhoodResourceTracker&) {
    shouldNotBeCalledPanic;
}
Neighbourhood generateRandomReassignNeighbourhood(const EmptyDomain&) {
    shouldNotBeCalledPanic;
}
const NeighbourhoodVec<EmptyDomain> NeighbourhoodGenList<EmptyDomain>::value =
    {};

const NeighbourhoodVec<EmptyDomain>
    NeighbourhoodGenList<EmptyDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<EmptyDomain>
    NeighbourhoodGenList<EmptyDomain>::splitNeighbourhoods = {};
