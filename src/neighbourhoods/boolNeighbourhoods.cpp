#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/bool.h"

void assignRandomValueInDomain(const BoolDomain&, BoolValue& val) {
    val.value = ((double)std::rand()) / RAND_MAX >= 0.5;
}
