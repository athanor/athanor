#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/set.h"
#include "utils/random.h"

using namespace std;
static ViolationContainer emptyViolations;

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

template <typename InnerDomainPtrType>
void setAddGenImpl(const SetDomain& domain, InnerDomainPtrType& innerDomainPtr,
                   int numberValsRequired,
                   std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    UInt innerDomainSize = getDomainSize(domain.inner);

    neighbourhoods.emplace_back(
        "setAdd", numberValsRequired,
        [innerDomainSize, &domain,
         &innerDomainPtr](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<SetValue>().front());
            if (val.numberElements() == domain.sizeAttr.maxSize) {
                ++params.stats.minorNodeCount;
                return;
            }
            auto newMember = constructValueFromDomain(*innerDomainPtr);
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit *
                                 calcNumberInsertionAttempts(
                                     val.numberElements(), innerDomainSize);
            debug_neighbourhood_action("Looking for value to add");
            bool success = false;
            do {
                assignRandomValueInDomain(*innerDomainPtr, *newMember,
                                          params.stats);
                success = val.tryAddMember(newMember, [&]() {
                    return params.parentCheck(params.vals);
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
void setAddGen(const SetDomain& domain, int numberValsRequired,
               std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setAddGenImpl(domain, innerDomainPtr, numberValsRequired,
                          neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void setCrossOverGenImpl(const SetDomain& domain, InnerDomainPtrType&,
                         int numberValsRequired,
                         std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    UInt innerDomainSize = getDomainSize(domain.inner);

    neighbourhoods.emplace_back(
        "setCrossOver", numberValsRequired,
        [innerDomainSize, &domain](NeighbourhoodParams& params) {
            auto& vals = params.getVals<SetValue>();
            debug_code(assert(vals.size() == 2));
            auto& fromVal = *(vals[0]);
            auto& toVal = *(vals[1]);
            if (fromVal.numberElements() == 0 || toVal.numberElements() == 0) {
                ++params.stats.minorNodeCount;
                return;
            }
            int numberTries = 0;
            const int tryLimit =
                params.parentCheckTryLimit *
                calcNumberInsertionAttempts(fromVal.numberElements(),
                                            innerDomainSize) *
                calcNumberInsertionAttempts(toVal.numberElements(),
                                            innerDomainSize);
            debug_neighbourhood_action("Looking for values to cross over");
            bool success = false;
            UInt fromIndexToMove, toIndexToMove;
            ValRef<InnerValueType> fromMemberToMove = nullptr,
                                   toMemberToMove = nullptr;
            do {
                ++params.stats.minorNodeCount;
                fromIndexToMove =
                    globalRandom<UInt>(0, fromVal.numberElements() - 1);
                toIndexToMove =
                    globalRandom<UInt>(0, toVal.numberElements() - 1);
                if (toVal.containsMember(
                        fromVal.getMembers<InnerViewType>()[fromIndexToMove]) ||
                    fromVal.containsMember(
                        toVal.getMembers<InnerViewType>()[toIndexToMove])) {
                    continue;
                }
                fromMemberToMove = assumeAsValue(
                    fromVal.getMembers<InnerViewType>()[fromIndexToMove]);
                toMemberToMove = assumeAsValue(
                    toVal.getMembers<InnerViewType>()[toIndexToMove]);
                HashType fromMemberHash =
                    fromVal.notifyPossibleMemberChange<InnerValueType>(
                        fromIndexToMove);
                HashType toMemberHash =
                    toVal.notifyPossibleMemberChange<InnerValueType>(
                        toIndexToMove);
                swapValAssignments(*fromMemberToMove, *toMemberToMove);
                success = fromVal
                              .tryMemberChange<InnerValueType>(
                                  fromIndexToMove, fromMemberHash,
                                  [&]() {
                                      return toVal
                                          .tryMemberChange<InnerValueType>(
                                              toIndexToMove, toMemberHash,
                                              [&]() {
                                                  return params.parentCheck(
                                                      params.vals);
                                              })
                                          .first;
                                  })
                              .first;
            } while (!success && ++numberTries < tryLimit);

            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find values to cross over, number tries="
                    << tryLimit);
                return;
            }

            debug_neighbourhood_action(
                "CrossOverd values: "
                << toVal.getMembers<InnerViewType>()[toIndexToMove] << " and "
                << fromVal.getMembers<InnerViewType>()[fromIndexToMove]);
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                HashType fromMemberHash =
                    fromVal.notifyPossibleMemberChange<InnerValueType>(
                        fromIndexToMove);
                HashType toMemberHash =
                    toVal.notifyPossibleMemberChange<InnerValueType>(
                        toIndexToMove);
                swapValAssignments(*fromMemberToMove, *toMemberToMove);
                fromVal.tryMemberChange<InnerValueType>(
                    fromIndexToMove, fromMemberHash, [&]() {
                        return toVal
                            .tryMemberChange<InnerValueType>(
                                toIndexToMove, toMemberHash,
                                [&]() { return true; })
                            .first;
                    });
            }
        });
}

void setCrossOverGen(const SetDomain& domain, int numberValsRequired,
                     std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setCrossOverGenImpl(domain, innerDomainPtr, numberValsRequired,
                                neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void setMoveGenImpl(const SetDomain& domain, InnerDomainPtrType&,
                    int numberValsRequired,
                    std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    UInt innerDomainSize = getDomainSize(domain.inner);

    neighbourhoods.emplace_back(
        "setMove", numberValsRequired,
        [innerDomainSize, &domain](NeighbourhoodParams& params) {
            auto& vals = params.getVals<SetValue>();
            debug_code(assert(vals.size() == 2));
            auto& fromVal = *(vals[0]);
            auto& toVal = *(vals[1]);
            if (fromVal.numberElements() == domain.sizeAttr.minSize ||
                toVal.numberElements() == domain.sizeAttr.maxSize) {
                ++params.stats.minorNodeCount;
                return;
            }
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit *
                                 calcNumberInsertionAttempts(
                                     toVal.numberElements(), innerDomainSize);
            debug_neighbourhood_action("Looking for value to move");
            bool success = false;
            do {
                ++params.stats.minorNodeCount;
                UInt indexToMove =
                    globalRandom<UInt>(0, fromVal.numberElements() - 1);
                if (toVal.containsMember(
                        fromVal.getMembers<InnerViewType>()[indexToMove])) {
                    continue;
                }
                auto memberToMove = assumeAsValue(
                    fromVal.getMembers<InnerViewType>()[indexToMove]);
                success = toVal.tryAddMember(memberToMove, [&]() {
                    return fromVal
                        .tryRemoveMember<InnerValueType>(
                            indexToMove,
                            [&]() { return params.parentCheck(params.vals); })
                        .first;
                });
            } while (!success && ++numberTries < tryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find value, number tries=" << tryLimit);
                return;
            }
            debug_neighbourhood_action(
                "Moved value: " << toVal.getMembers<InnerViewType>().back());
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                auto member =
                    toVal
                        .tryRemoveMember<InnerValueType>(
                            toVal.numberElements() - 1, []() { return true; })
                        .second;
                fromVal.tryAddMember(member, [&]() { return true; });
            }
        });
}
void setMoveGen(const SetDomain& domain, int numberValsRequired,
                std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setMoveGenImpl(domain, innerDomainPtr, numberValsRequired,
                           neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void setRemoveGenImpl(const SetDomain& domain, InnerDomainPtrType&,
                      int numberValsRequired,
                      std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    neighbourhoods.emplace_back(
        "setRemove", numberValsRequired, [&](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<SetValue>().front());
            if (val.numberElements() == domain.sizeAttr.minSize) {
                ++params.stats.minorNodeCount;
                return;
            }
            size_t indexToRemove;
            int numberTries = 0;
            ValRef<InnerValueType> removedMember(nullptr);
            bool success = false;
            debug_neighbourhood_action("Looking for value to remove");
            do {
                ++params.stats.minorNodeCount;
                indexToRemove =
                    globalRandom<size_t>(0, val.numberElements() - 1);
                debug_log("trying to remove index "
                          << indexToRemove << " from set " << val.view());
                std::pair<bool, ValRef<InnerValueType>> removeStatus =
                    val.tryRemoveMember<InnerValueType>(indexToRemove, [&]() {
                        return params.parentCheck(params.vals);
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
                val.tryAddMember(std::move(removedMember),
                                 []() { return true; });
            }
        });
}

void setRemoveGen(const SetDomain& domain, int numberValsRequired,
                  std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setRemoveGenImpl(domain, innerDomainPtr, numberValsRequired,
                             neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void setLiftSingleGenImpl(const SetDomain& domain, const InnerDomainPtrType&,
                          int numberValsRequired,
                          std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(1, domain.inner, innerDomainNeighbourhoods);
    UInt innerDomainSize = getDomainSize(domain.inner);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "setLiftSingle_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)},
             innerDomainSize](NeighbourhoodParams& params) {
                auto& val = *(params.getVals<SetValue>().front());
                if (val.numberElements() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                ViolationContainer& vioContainerAtThisLevel =
                    params.vioContainer.hasChildViolation(val.id)
                        ? params.vioContainer.childViolations(val.id)
                        : emptyViolations;
                UInt indexToChange = vioContainerAtThisLevel.selectRandomVar(
                    val.numberElements() - 1);
                HashType oldHash =
                    val.notifyPossibleMemberChange<InnerValueType>(
                        indexToChange);
                ParentCheckCallBack parentCheck =
                    [&](const AnyValVec& newValue) {
                        HashType newHash = getValueHash(
                            mpark::get<ValRefVec<InnerValueType>>(newValue)
                                .front());
                        if (val.memberHashes.count(newHash)) {
                            return false;
                        }
                        auto statusHashPair =
                            val.tryMemberChange<InnerValueType>(
                                indexToChange, oldHash, [&]() {
                                    return params.parentCheck(params.vals);
                                });
                        oldHash = statusHashPair.second;
                        return statusHashPair.first;
                    };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        oldHash =
                            val.notifyPossibleMemberChange<InnerValueType>(
                                indexToChange);
                    }
                    return !requiresRevert;
                };
                AnyValVec changingMembers;
                auto& changingMembersImpl =
                    changingMembers.emplace<ValRefVec<InnerValueType>>();
                changingMembersImpl.emplace_back(
                    val.member<InnerValueType>(indexToChange));
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck,
                    calcNumberInsertionAttempts(val.numberElements(),
                                                innerDomainSize),
                    changingMembers, params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryMemberChange<InnerValueType>(indexToChange, oldHash,
                                                        [&]() { return true; });
                }
            });
    }
}

void setLiftSingleGen(const SetDomain& domain, int numberValsRequired,
                      std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setLiftSingleGenImpl(domain, innerDomainPtr, numberValsRequired,
                                 neighbourhoods);
        },
        domain.inner);
}

template <typename InnerValueType>
bool setDoesNotContain(SetValue& val,
                       const ValRefVec<InnerValueType>& newMembers) {
    size_t lastInsertedIndex = 0;
    do {
        HashType hash = getValueHash(newMembers[lastInsertedIndex]);
        bool inserted = val.memberHashes.insert(hash).second;
        if (!inserted) {
            break;
        }
    } while (++lastInsertedIndex < newMembers.size());
    for (size_t i = 0; i < lastInsertedIndex; i++) {
        val.memberHashes.erase(getValueHash(newMembers[i]));
    }
    return lastInsertedIndex == newMembers.size();
}
template <typename InnerDomainPtrType>
void setLiftMultipleGenImpl(const SetDomain& domain, const InnerDomainPtrType&,
                            int numberValsRequired,
                            std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(0, domain.inner, innerDomainNeighbourhoods);
    UInt innerDomainSize = getDomainSize(domain.inner);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        if (innerNh.numberValsRequired < 2) {
            continue;
        }
        neighbourhoods.emplace_back(
            "setLiftMultiple_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)},
             innerNhNumberValsRequired{innerNh.numberValsRequired},
             innerDomainSize](NeighbourhoodParams& params) {
                auto& val = *(params.getVals<SetValue>().front());
                if (val.numberElements() < (size_t)innerNhNumberValsRequired) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                ViolationContainer& vioContainerAtThisLevel =
                    params.vioContainer.hasChildViolation(val.id)
                        ? params.vioContainer.childViolations(val.id)
                        : emptyViolations;
                std::vector<UInt> indicesToChange =
                    vioContainerAtThisLevel.selectRandomVars(
                        val.numberElements() - 1, innerNhNumberValsRequired);
                debug_log(indicesToChange);
                std::vector<HashType> oldHashes;
                val.notifyPossibleMembersChange<InnerValueType>(indicesToChange,
                                                                oldHashes);
                ParentCheckCallBack parentCheck =
                    [&](const AnyValVec& newMembers) {
                        auto& newMembersImpl =
                            mpark::get<ValRefVec<InnerValueType>>(newMembers);
                        return setDoesNotContain(val, newMembersImpl) &&
                               val.tryMembersChange<InnerValueType>(
                                   indicesToChange, oldHashes, [&]() {
                                       return params.parentCheck(params.vals);
                                   });
                    };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        val.notifyPossibleMembersChange<InnerValueType>(
                            indicesToChange, oldHashes);
                    }
                    return !requiresRevert;
                };
                AnyValVec changingMembers;
                auto& changingMembersImpl =
                    changingMembers.emplace<ValRefVec<InnerValueType>>();
                for (UInt indexToChange : indicesToChange) {
                    changingMembersImpl.emplace_back(
                        val.member<InnerValueType>(indexToChange));
                }
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck,
                    calcNumberInsertionAttempts(val.numberElements(),
                                                innerDomainSize),
                    changingMembers, params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryMembersChange<InnerValueType>(
                        indicesToChange, oldHashes, [&]() { return true; });
                }
            });
    }
}

void setLiftMultipleGen(const SetDomain& domain, int numberValsRequired,
                        std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setLiftMultipleGenImpl(domain, innerDomainPtr, numberValsRequired,
                                   neighbourhoods);
        },
        domain.inner);
}

void setAssignRandomGen(const SetDomain& domain, int numberValsRequired,
                        std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "setAssignRandom", numberValsRequired,
        [&domain](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<SetValue>().front());
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            debug_neighbourhood_action(
                "Assigning random value: original value is " << asView(val));
            auto backup = deepCopy(val);
            backup->container = val.container;
            auto newValue = constructValueFromDomain(domain);
            newValue->container = val.container;
            bool success = false;
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

const NeighbourhoodVec<SetDomain> NeighbourhoodGenList<SetDomain>::value = {
    {1, setLiftSingleGen},    //
    {1, setLiftMultipleGen},  //
    {1, setAddGen},          {1, setRemoveGen},
    {2, setMoveGen},         {2, setCrossOverGen}};
