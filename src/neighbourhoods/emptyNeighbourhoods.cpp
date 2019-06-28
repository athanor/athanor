#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/emptyVal.h"
#include "utils/random.h"

using namespace std;

template <>
void assignRandomValueInDomain<EmptyDomain>(const EmptyDomain&, EmptyValue&,
                                            StatsContainer&) {
    shouldNotBeCalledPanic;
}

const NeighbourhoodVec<EmptyDomain> NeighbourhoodGenList<EmptyDomain>::value =
    {};

const NeighbourhoodVec<EmptyDomain>
    NeighbourhoodGenList<EmptyDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<EmptyDomain>
    NeighbourhoodGenList<EmptyDomain>::splitNeighbourhoods = {};
