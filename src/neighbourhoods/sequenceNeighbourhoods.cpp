#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/sequence.h"
#include "utils/random.h"

using namespace std;
static ViolationDescription emptyViolations;

template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const SequenceDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   SequenceValue& val) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    size_t newNumberElements =
        globalRandom(domain.sizeAttr.minSize, domain.sizeAttr.maxSize);
    // clear sequence and populate with new random elements
    while (val.numberElements() > 0) {
        val.removeMember<InnerValueType>(val.numberElements() - 1);
    }
    while (newNumberElements > val.numberElements()) {
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        assignRandomValueInDomain(*innerDomainPtr, *newMember);
        val.addMember(val.numberElements(), newMember);
    }
}

template <>
void assignRandomValueInDomain<SequenceDomain>(const SequenceDomain& domain,
                                               SequenceValue& val) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void sequenceLiftSingleGenImpl(const SequenceDomain& domain,
                               const InnerDomainPtrType& innerDomainPtr,
                               std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(domain.inner, innerDomainNeighbourhoods);
    UInt innerDomainSize = getDomainSize(domain.inner);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "sequenceLiftSingle_" + innerNh.name,
            [innerNhApply{std::move(innerNh.apply)}, innerDomainSize, &domain,
             &innerDomainPtr](NeighbourhoodParams& params) {
                auto& val = *mpark::get<ValRef<SequenceValue>>(params.primary);
                if (val.numberElements() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                ViolationDescription& vioDescAtThisLevel =
                    params.vioDesc.hasChildViolation(val.id)
                        ? params.vioDesc.childViolations(val.id)
                        : emptyViolations;
                UInt indexToChange =
                    (vioDescAtThisLevel.getTotalViolation() != 0)
                        ? vioDescAtThisLevel.selectRandomVar()
                        : globalRandom<UInt>(0, val.numberElements() - 1);
                val.notifyPossibleSubsequenceChange<InnerValueType>(
                    indexToChange, indexToChange + 1);
                ParentCheckCallBack parentCheck = [&](const AnyValRef&) {
                    return val.trySubsequenceChange<InnerValueType>(
                        indexToChange, indexToChange + 1,
                        [&]() { return params.parentCheck(params.primary); });
                };
                bool requiresRevert = false;
                AnyValRef changingMember =
                    val.member<InnerValueType>(indexToChange);
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        val.notifyPossibleSubsequenceChange<InnerValueType>(
                            indexToChange, indexToChange + 1);
                    }
                    return !requiresRevert;
                };
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck, 1, changingMember,
                    params.stats, vioDescAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.trySubsequenceChange<InnerValueType>(
                        indexToChange, indexToChange + 1,
                        [&]() { return true; });
                }
            });
    }
}

void sequenceLiftSingleGen(const SequenceDomain& domain,
                           std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            sequenceLiftSingleGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void sequenceAddGenImpl(const SequenceDomain& domain,
                        InnerDomainPtrType& innerDomainPtr,
                        std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;

    neighbourhoods.emplace_back(
        "sequenceAdd", [&domain, &innerDomainPtr](NeighbourhoodParams& params) {
            auto& val = *mpark::get<ValRef<SequenceValue>>(params.primary);
            if (val.numberElements() == domain.sizeAttr.maxSize) {
                ++params.stats.minorNodeCount;
                return;
            }
            auto newMember = constructValueFromDomain(*innerDomainPtr);
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            debug_neighbourhood_action("Looking for value to add");
            bool success;
            do {
                ++params.stats.minorNodeCount;
                assignRandomValueInDomain(*innerDomainPtr, *newMember);
                size_t indexOfNewMember =
                    globalRandom<UInt>(0, val.numberElements());
                success = val.tryAddMember(indexOfNewMember, newMember, [&]() {
                    return params.parentCheck(params.primary);
                });
            } while (!success && ++numberTries < tryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find value, number tries=" << tryLimit);
                return;
            }
            debug_neighbourhood_action("Added value: " << newMember);
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                val.tryRemoveMember<InnerValueType>(val.numberElements() - 1,
                                                    []() { return true; });
            }
        });
}

void sequenceAddGen(const SequenceDomain& domain,
                    std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            sequenceAddGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void sequenceRemoveGenImpl(const SequenceDomain& domain, InnerDomainPtrType&,
                           std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    neighbourhoods.emplace_back(
        "sequenceRemove", [&](NeighbourhoodParams& params) {
            ++params.stats.minorNodeCount;
            auto& val = *mpark::get<ValRef<SequenceValue>>(params.primary);
            if (val.numberElements() == domain.sizeAttr.minSize) {
                ++params.stats.minorNodeCount;
                return;
            }
            size_t indexToRemove;
            int numberTries = 0;
            ValRef<InnerValueType> removedMember(nullptr);
            bool success;
            debug_neighbourhood_action("Looking for value to remove");
            do {
                indexToRemove =
                    globalRandom<size_t>(0, val.numberElements() - 1);
                std::pair<bool, ValRef<InnerValueType>> removeStatus =
                    val.tryRemoveMember<InnerValueType>(indexToRemove, [&]() {
                        return params.parentCheck(params.primary);
                    });
                success = removeStatus.first;
                if (success) {
                    removedMember = std::move(removeStatus.second);
                    debug_neighbourhood_action("Removed " << removedMember);
                }
            } while (!success && ++numberTries < params.parentCheckTryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find value, number tries=" << numberTries);
                return;
            }
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                val.tryAddMember(indexToRemove, std::move(removedMember),
                                 []() { return true; });
            }
        });
}

void sequenceRemoveGen(const SequenceDomain& domain,
                       std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            sequenceRemoveGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

void sequenceAssignRandomGen(const SequenceDomain& domain,
                             std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "sequenceAssignRandom", [&domain](NeighbourhoodParams& params) {
            auto& val = *mpark::get<ValRef<SequenceValue>>(params.primary);
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
                ++params.stats.minorNodeCount;
                assignRandomValueInDomain(domain, *newValue);
                success = val.tryAssignNewValue(*newValue, [&]() {
                    return params.parentCheck(params.primary);
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

const NeighbourhoodVec<SequenceDomain>
    NeighbourhoodGenList<SequenceDomain>::value = {
        sequenceLiftSingleGen };
