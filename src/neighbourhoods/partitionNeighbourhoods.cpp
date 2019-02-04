#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/partitionVal.h"
#include "utils/random.h"

using namespace std;

template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const PartitionDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   PartitionValue& val, StatsContainer& stats) {
    // clear partition and populate with new random elements
    val.silentClear();
    stats.minorNodeCount += val.memberPartMap.size();
    vector<UInt> parts;
    size_t partSize = domain.numberElements / domain.numberParts;
    for (size_t i = 0; i < domain.numberParts; i++) {
        for (size_t j = 0; j < partSize; j++) {
            parts.emplace_back(i);
        }
    }
    shuffle(begin(parts), end(parts), globalRandomGenerator);
    while (domain.numberElements > val.numberElements()) {
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        assignRandomValueInDomain(*innerDomainPtr, *newMember, stats);
        if (val.addMember(parts.back(), newMember)) {
            parts.pop_back();
        }
        // add member may reject elements, not to worry, while loop will simply
        // continue
    }
}

template <>
void assignRandomValueInDomain<PartitionDomain>(const PartitionDomain& domain,
                                                PartitionValue& val,
                                                StatsContainer& stats) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val, stats);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void partitionContainingPartsSwapGenImpl(
    const PartitionDomain& domain, InnerDomainPtrType&, int numberValsRequired,
    std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;

    neighbourhoods.emplace_back(
        "partitionPositionsSwap", numberValsRequired,
        [&domain](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<PartitionValue>().front());
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit *
                                 calcNumberInsertionAttempts(
                                     domain.numberElements / domain.numberParts,
                                     domain.numberElements);
            debug_neighbourhood_action("Looking for indices to swap");

            bool success;
            UInt index1, index2;
            do {
                ++params.stats.minorNodeCount;
                index1 = globalRandom<UInt>(0, domain.numberElements - 1);
                index2 = globalRandom<UInt>(0, domain.numberElements - 1);
                index2 = (index1 + index2) % domain.numberElements;
                if (val.memberPartMap[index1] == val.memberPartMap[index2]) {
                    success = false;
                    continue;
                }
                success = val.trySwapContainingParts<InnerValueType>(
                    index1, index2,
                    [&]() { return params.parentCheck(params.vals); });
            } while (!success && ++numberTries < tryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find containing parts to swap, number tries="
                    << tryLimit);
                return;
            }
            debug_neighbourhood_action(
                "positions swapped: " << index1 << " and " << index2);
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                val.trySwapContainingParts<InnerValueType>(
                    index1, index2, []() { return true; });
            }
        });
}

void partitionContainingPartsSwapGen(
    const PartitionDomain& domain, int numberValsRequired,
    std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            partitionContainingPartsSwapGenImpl(
                domain, innerDomainPtr, numberValsRequired, neighbourhoods);
        },
        domain.inner);
}

void partitionAssignRandomGen(const PartitionDomain& domain,
                              int numberValsRequired,
                              std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "partitionAssignRandom", numberValsRequired,
        [&domain](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<PartitionValue>().front());
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

const NeighbourhoodVec<PartitionDomain>
    NeighbourhoodGenList<PartitionDomain>::value = {
        {1, partitionContainingPartsSwapGen},
};

const NeighbourhoodVec<PartitionDomain>
    NeighbourhoodGenList<PartitionDomain>::mergeNeighbourhoods = {};

const NeighbourhoodVec<PartitionDomain>
    NeighbourhoodGenList<PartitionDomain>::splitNeighbourhoods = {};
