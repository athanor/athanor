#include "neighbourhoods/neighbourhoods.h"
#include <cassert>
#include <random>
#include "search/solver.h"
#include "search/statsContainer.h"
using namespace std;
UInt64 NeighbourhoodResourceTracker::remainingResource() {
    return resourceLimit - resourceConsumed;
}
bool NeighbourhoodResourceTracker::hasResource() {
    return remainingResource() > 0;
}

bool NeighbourhoodResourceTracker::requestResource() {
    if (!hasResource()) {
        return false;
    }
    ++resourceConsumed;
    return true;
}
UInt64 NeighbourhoodResourceTracker::getResourceConsumed() {
    return resourceConsumed;
}

// param innerDomain is the domain of  the elements that are to be created.
lib::optional<UInt64> NeighbourhoodResourceTracker::randomNumberElements(
    size_t minSize, size_t maxSize, const AnyDomainRef& innerDomain) {
    if (!hasResource()) {
        return lib::nullopt;
    }
    UInt64 elementResourceLowerBound = getResourceLowerBound(innerDomain);
    UInt64 maxNumberElements =
        remainingResource() / elementResourceLowerBound + 1;
    if (maxNumberElements < minSize) {
        return lib::nullopt;
    }
    return globalRandom<UInt64>(minSize,
                                min(maxSize, (size_t)maxNumberElements));
}

NeighbourhoodResourceTracker::Reserved NeighbourhoodResourceTracker::reserve(
    size_t minNumberElements, const AnyDomainRef& elementDomain,
    size_t numberElementsSoFar) {
    if (numberElementsSoFar >= minNumberElements) {
        return Reserved(*this, 0);
    }
    // reserve enough for number of elements yet to be assigned minus 1
    size_t numberElementsToReserve =
        minNumberElements - numberElementsSoFar - 1;
    size_t amount =
        numberElementsToReserve * getResourceLowerBound(elementDomain);
    return (amount <= resourceLimit) ? Reserved(*this, amount)
                                     : Reserved(*this, resourceLimit);
}
static const double MULTIPLIER = 1.1;
static const UInt MIN_CONSTANT = 500;
#define AllocatorConstructor(name)                                  \
    template <>                                                     \
    NeighbourhoodResourceAllocator::NeighbourhoodResourceAllocator( \
        const name##Domain& domain)                                 \
        : resource(getResourceLowerBound(domain) * MULTIPLIER +     \
                   MIN_CONSTANT) {}

buildForAllTypes(AllocatorConstructor, );
#undef AllocatorConstructor

NeighbourhoodResourceTracker
NeighbourhoodResourceAllocator::requestLargerResource() {
    NeighbourhoodResourceTracker tracker((UInt64(resource)));
    resource *= MULTIPLIER;
    return tracker;
}
