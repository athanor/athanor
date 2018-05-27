#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/function.h"
#include "utils/random.h"

using namespace std;
static ViolationContainer emptyViolations;

template <>
void assignRandomValueInDomain<FunctionDomain>(const FunctionDomain&,
                                               FunctionValue&,
                                               StatsContainer&) {
    todoImpl();
}
const NeighbourhoodVec<FunctionDomain>
    NeighbourhoodGenList<FunctionDomain>::value = {};
