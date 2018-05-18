#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/set.h"
#include "utils/random.h"

using namespace std;
static ViolationDescription emptyViolations;

template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const SetDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   SetValue& val, StatsContainer& stats) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    size_t newNumberElements =
        globalRandom(domain.sizeAttr.minSize, domain.sizeAttr.maxSize);
    // clear set and populate with new random elements
    while (val.numberElements() > 0) {
        val.removeMember<InnerValueType>(val.numberElements() - 1);
        ++stats.minorNodeCount;
    }
    while (newNumberElements > val.numberElements()) {
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        do {
            assignRandomValueInDomain(*innerDomainPtr, *newMember, stats);
        } while (!val.addMember(newMember));
    }
}

template <>
void assignRandomValueInDomain<SetDomain>(const SetDomain& domain,
                                          SetValue& val,
                                          StatsContainer& stats) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val, stats);
        },
        domain.inner);
}

const int NUMBER_TRIES_CONSTANT_MULTIPLIER = 2;
inline int getTryLimit(UInt numberMembers, UInt domainSize) {
    double successChance = (domainSize - numberMembers) / (double)domainSize;
    return (int)(ceil(NUMBER_TRIES_CONSTANT_MULTIPLIER / successChance));
}
template <typename InnerDomainPtrType>
void setAddGenImpl(const SetDomain& domain, InnerDomainPtrType& innerDomainPtr,
                   std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    UInt innerDomainSize = getDomainSize(domain.inner);

    neighbourhoods.emplace_back(
        "setAdd", [innerDomainSize, &domain,
                   &innerDomainPtr](NeighbourhoodParams& params) {
            auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
            if (val.numberElements() == domain.sizeAttr.maxSize) {
                ++params.stats.minorNodeCount;
                return;
            }
            auto newMember = constructValueFromDomain(*innerDomainPtr);
            int numberTries = 0;
            const int tryLimit =
                params.parentCheckTryLimit *
                getTryLimit(val.numberElements(), innerDomainSize);
            debug_neighbourhood_action("Looking for value to add");
            bool success;
            do {
                assignRandomValueInDomain(*innerDomainPtr, *newMember,
                                          params.stats);
                success = val.tryAddMember(newMember, [&]() {
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
void setAddGen(const SetDomain& domain,
               std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setAddGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void setRemoveGenImpl(const SetDomain& domain, InnerDomainPtrType&,
                      std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    neighbourhoods.emplace_back("setRemove", [&](NeighbourhoodParams& params) {
        auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
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
            ++params.stats.minorNodeCount;
            indexToRemove = globalRandom<size_t>(0, val.numberElements() - 1);
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
            val.tryAddMember(std::move(removedMember), []() { return true; });
        }
    });
}

void setRemoveGen(const SetDomain& domain,
                  std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setRemoveGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void setLiftSingleGenImpl(const SetDomain& domain,
                          const InnerDomainPtrType& innerDomainPtr,
                          std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(domain.inner, innerDomainNeighbourhoods);
    UInt innerDomainSize = getDomainSize(domain.inner);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "setLiftSingle_" + innerNh.name,
            [innerNhApply{std::move(innerNh.apply)}, innerDomainSize, &domain,
             &innerDomainPtr](NeighbourhoodParams& params) {
                auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
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
                val.notifyPossibleMemberChange<InnerValueType>(indexToChange);
                ParentCheckCallBack parentCheck =
                    [&](const AnyValRef& newValue) {
                        HashType newHash = getValueHash(newValue);
                        return !val.hashIndexMap.count(newHash) &&
                               val.tryMemberChange<InnerValueType>(
                                   indexToChange, [&]() {
                                       return params.parentCheck(
                                           params.primary);
                                   });
                    };
                bool requiresRevert = false;
                AnyValRef changingMember =
                    val.member<InnerValueType>(indexToChange);
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        val.notifyPossibleMemberChange<InnerValueType>(
                            indexToChange);
                    }
                    return !requiresRevert;
                };
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck,
                    getTryLimit(val.numberElements(), innerDomainSize),
                    changingMember, params.stats, vioDescAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryMemberChange<InnerValueType>(indexToChange,
                                                        [&]() { return true; });
                }
            });
    }
}

void setLiftSingleGen(const SetDomain& domain,
                      std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setLiftSingleGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

void setAssignRandomGen(const SetDomain& domain,
                        std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "setAssignRandom", [&domain](NeighbourhoodParams& params) {
            auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
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

const NeighbourhoodVec<SetDomain> NeighbourhoodGenList<SetDomain>::value = {
    setLiftSingleGen, setAddGen, setRemoveGen};
