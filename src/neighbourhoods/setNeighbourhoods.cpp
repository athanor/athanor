#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/setVal.h"
#include "utils/random.h"

using namespace std;

template <typename InnerDomainPtrType>
bool assignRandomValueInDomainImpl(const SetDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   SetValue& val,
                                   NeighbourhoodResourceTracker& resource) {
    if (!resource.requestResource()) {
        return false;
    }
    auto newNumberElementsOption = resource.randomNumberElements(
        domain.sizeAttr.minSize, domain.sizeAttr.maxSize, innerDomainPtr);
    if (!newNumberElementsOption) {
        return false;
    }
    size_t newNumberElements = *newNumberElementsOption;
    // clear set and populate with new random elements
    val.silentClear();
    while (newNumberElements > val.numberElements()) {
        auto reserved = resource.reserve(domain.sizeAttr.minSize,
                                         innerDomainPtr, val.numberElements());
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        do {
            bool success = assignRandomValueInDomain(*innerDomainPtr,
                                                     *newMember, resource);
            if (!success) {
                return val.numberElements() >= domain.sizeAttr.minSize;
                // still  successful if  we achieved minimum size
            }
        } while (!val.addMember(newMember));
    }
    return true;
}

template <>
bool assignRandomValueInDomain<SetDomain>(
    const SetDomain& domain, SetValue& val,
    NeighbourhoodResourceTracker& resource) {
    return lib::visit(
        [&](auto& innerDomainPtr) {
            return assignRandomValueInDomainImpl(domain, innerDomainPtr, val,
                                                 resource);
        },
        domain.inner);
}

template <typename InnerDomain>
struct SetAdd : public NeighbourhoodFunc<SetDomain, 1, SetAdd<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SetDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SetAdd(const SetDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SetAdd"; }
    static bool matches(const SetDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE;
    }
    void apply(NeighbourhoodParams& params, SetValue& val) {
        if (val.numberElements() == domain.sizeAttr.maxSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        auto newMember = constructValueFromDomain(innerDomain);
        int numberTries = 0;
        const int tryLimit =
            params.parentCheckTryLimit *
            calcNumberInsertionAttempts(val.numberElements(), innerDomainSize);
        debug_neighbourhood_action("Looking for value to add");
        bool success = false;
        NeighbourhoodResourceAllocator resourceAllocator(innerDomain);
        do {
            auto resource = resourceAllocator.requestLargerResource();
            success =
                assignRandomValueInDomain(innerDomain, *newMember, resource);
            params.stats.minorNodeCount += resource.getResourceConsumed();
            success = success && val.tryAddMember(newMember, [&]() {
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
    }
};

template <typename InnerDomain>
struct SetCrossover
    : public NeighbourhoodFunc<SetDomain, 2, SetCrossover<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SetDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SetCrossover(const SetDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SetCrossover"; }
    static bool matches(const SetDomain&) { return true; }
    void apply(NeighbourhoodParams& params, SetValue& fromVal,
               SetValue& toVal) {
        if (fromVal.numberElements() == 0 || toVal.numberElements() == 0) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit *
                             calcNumberInsertionAttempts(
                                 fromVal.numberElements(), innerDomainSize) *
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
            toIndexToMove = globalRandom<UInt>(0, toVal.numberElements() - 1);
            if (toVal.containsMember(
                    fromVal.getMembers<InnerViewType>()[fromIndexToMove]) ||
                fromVal.containsMember(
                    toVal.getMembers<InnerViewType>()[toIndexToMove])) {
                continue;
            }
            fromMemberToMove = assumeAsValue(
                fromVal.getMembers<InnerViewType>()[fromIndexToMove]);
            toMemberToMove =
                assumeAsValue(toVal.getMembers<InnerViewType>()[toIndexToMove]);
            swapValAssignments(*fromMemberToMove, *toMemberToMove);
            success =
                fromVal.tryMemberChange<InnerValueType>(fromIndexToMove, [&]() {
                    return toVal.tryMemberChange<InnerValueType>(
                        toIndexToMove, [&]() {
                            if (params.parentCheck(params.vals)) {
                                return true;
                            } else {
                                swapValAssignments(*fromMemberToMove,
                                                   *toMemberToMove);
                                return false;
                            }
                        });
                });
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
            swapValAssignments(*fromMemberToMove, *toMemberToMove);
            fromVal.tryMemberChange<InnerValueType>(fromIndexToMove, [&]() {
                return toVal.tryMemberChange<InnerValueType>(
                    toIndexToMove, [&]() { return true; });
            });
        }
    }
};

template <typename InnerDomain>
struct SetMove : public NeighbourhoodFunc<SetDomain, 2, SetMove<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SetDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SetMove(const SetDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}

    static string getName() { return "SetMove"; }
    static bool matches(const SetDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE;
    }

    void apply(NeighbourhoodParams& params, SetValue& fromVal,
               SetValue& toVal) {
        if (fromVal.numberElements() == domain.sizeAttr.minSize ||
            toVal.numberElements() == domain.sizeAttr.maxSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit *
                             calcNumberInsertionAttempts(toVal.numberElements(),
                                                         innerDomainSize);
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
            auto memberToMove =
                assumeAsValue(fromVal.getMembers<InnerViewType>()[indexToMove]);
            success = toVal.tryAddMember(memberToMove, [&]() {
                return fromVal
                    .tryRemoveMember<InnerValueType>(
                        indexToMove,
                        [&]() { return params.parentCheck(params.vals); })
                    .has_value();
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
            auto member = *toVal.tryRemoveMember<InnerValueType>(
                toVal.numberElements() - 1, []() { return true; });
            fromVal.tryAddMember(member, [&]() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SetRemove
    : public NeighbourhoodFunc<SetDomain, 1, SetRemove<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;

    const SetDomain& domain;
    SetRemove(const SetDomain& domain) : domain(domain) {}
    static string getName() { return "SetRemove"; }
    static bool matches(const SetDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE;
    }
    void apply(NeighbourhoodParams& params, SetValue& val) {
        if (val.numberElements() == domain.sizeAttr.minSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        size_t indexToRemove;
        int numberTries = 0;
        lib::optional<ValRef<InnerValueType>> removedMember;
        bool success = false;
        debug_neighbourhood_action("Looking for value to remove");
        do {
            ++params.stats.minorNodeCount;
            indexToRemove = globalRandom<size_t>(0, val.numberElements() - 1);
            debug_log("trying to remove index " << indexToRemove << " from set "
                                                << val.view());
            removedMember = val.tryRemoveMember<InnerValueType>(
                indexToRemove,
                [&]() { return params.parentCheck(params.vals); });
            success = removedMember.has_value();
            if (success) {
                debug_neighbourhood_action("Removed " << *removedMember);
            }
        } while (!success && ++numberTries < params.parentCheckTryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find value, number tries=" << numberTries);
            return;
        }
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.tryAddMember(*removedMember, []() { return true; });
        }
    }
};

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
            "SetLiftSingle_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)},
             innerDomainSize](NeighbourhoodParams& params) {
                auto& val = *(params.getVals<SetValue>().front());
                if (val.numberElements() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id);
                UInt indexToChange = vioContainerAtThisLevel.selectRandomVar(
                    val.numberElements() - 1);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&
                                                          newValue) {
                    HashType newHash = getValueHash(
                        lib::get<ValRefVec<InnerValueType>>(newValue).front());
                    if (val.hashIndexMap.count(newHash)) {
                        return false;
                    }
                    return val.tryMemberChange<InnerValueType>(
                        indexToChange,
                        [&]() { return params.parentCheck(params.vals); });
                };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
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
                    val.tryMemberChange<InnerValueType>(indexToChange,
                                                        [&]() { return true; });
                }
            });
    }
}

void setLiftSingleGen(const SetDomain& domain, int numberValsRequired,
                      std::vector<Neighbourhood>& neighbourhoods) {
    lib::visit(
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
        bool inserted = val.hashIndexMap.emplace(hash, 0).second;
        if (!inserted) {
            break;
        }
    } while (++lastInsertedIndex < newMembers.size());
    for (size_t i = 0; i < lastInsertedIndex; i++) {
        val.hashIndexMap.erase(getValueHash(newMembers[i]));
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
            "SetLiftMultiple_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)},
             innerNhNumberValsRequired{innerNh.numberValsRequired},
             innerDomainSize](NeighbourhoodParams& params) {
                auto& val = *(params.getVals<SetValue>().front());
                if (val.numberElements() < (size_t)innerNhNumberValsRequired) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id);
                std::vector<UInt> indicesToChange =
                    vioContainerAtThisLevel.selectRandomVars(
                        val.numberElements() - 1, innerNhNumberValsRequired);
                debug_log(indicesToChange);
                ParentCheckCallBack parentCheck =
                    [&](const AnyValVec& newMembers) {
                        auto& newMembersImpl =
                            lib::get<ValRefVec<InnerValueType>>(newMembers);
                        return setDoesNotContain(val, newMembersImpl) &&
                               val.tryMembersChange<InnerValueType>(
                                   indicesToChange, [&]() {
                                       return params.parentCheck(params.vals);
                                   });
                    };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
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
                        indicesToChange, [&]() { return true; });
                }
            });
    }
}

void setLiftMultipleGen(const SetDomain& domain, int numberValsRequired,
                        std::vector<Neighbourhood>& neighbourhoods) {
    lib::visit(
        [&](const auto& innerDomainPtr) {
            setLiftMultipleGenImpl(domain, innerDomainPtr, numberValsRequired,
                                   neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomain>
struct SetAssignRandom
    : public NeighbourhoodFunc<SetDomain, 1, SetAssignRandom<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;

    const SetDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SetAssignRandom(const SetDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}

    static string getName() { return "SetAssignRandom"; }
    static bool matches(const SetDomain&) { return true; }
    void apply(NeighbourhoodParams& params, SetValue& val) {
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Assigning random value: original value is "
                                   << asView(val));
        auto backup = deepCopy(val);
        backup->container = val.container;
        auto newValue = constructValueFromDomain(domain);
        newValue->container = val.container;
        bool success = false;
        NeighbourhoodResourceAllocator resourceAllocator(domain);
        do {
            auto resource = resourceAllocator.requestLargerResource();
            success = assignRandomValueInDomain(domain, *newValue, resource);
            params.stats.minorNodeCount += resource.getResourceConsumed();
            success = success && val.tryAssignNewValue(*newValue, [&]() {
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
    }
};

template <>
const AnyDomainRef getInner<SetDomain>(const SetDomain& domain) {
    return domain.inner;
}

const NeighbourhoodVec<SetDomain> NeighbourhoodGenList<SetDomain>::value = {
    {1, setLiftSingleGen},                             //
    {1, setLiftMultipleGen},                           //
    {1, generateForAllTypes<SetDomain, SetAdd>},       //
    {1, generateForAllTypes<SetDomain, SetRemove>},    //
    {2, generateForAllTypes<SetDomain, SetMove>},      //
    {2, generateForAllTypes<SetDomain, SetCrossover>}  //
    //    {1, generateForAllTypes<SetDomain, SetAssignRandom>}  //
};

const NeighbourhoodVec<SetDomain>
    NeighbourhoodGenList<SetDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<SetDomain>
    NeighbourhoodGenList<SetDomain>::splitNeighbourhoods = {};
