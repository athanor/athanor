#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/bool.h"
#include "utils/random.h"

template <>
void assignRandomValueInDomain<BoolDomain>(const BoolDomain&, BoolValue& val) {
    val.changeValue([&]() { val.violation = globalRandom(0, 1); });
}

const NeighbourhoodGenList<BoolDomain>::type
    NeighbourhoodGenList<BoolDomain>::value = {};
