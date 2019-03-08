#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/mSetVal.h"
#include "utils/random.h"

using namespace std;

template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const MSetDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   MSetValue& val, StatsContainer& stats) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    size_t newNumberElements =
        randomSize(domain.sizeAttr.minSize, domain.sizeAttr.maxSize);
    // clear mSet and populate with new random elements
    while (val.numberElements() > 0) {
        val.removeMember<InnerValueType>(val.numberElements() - 1);
        ++stats.minorNodeCount;
    }
    while (newNumberElements > val.numberElements()) {
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        assignRandomValueInDomain(*innerDomainPtr, *newMember, stats);
        val.addMember(newMember);
    }
}

template <>
void assignRandomValueInDomain<MSetDomain>(const MSetDomain& domain,
                                           MSetValue& val,
                                           StatsContainer& stats) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val, stats);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void mSetLiftSingleGenImpl(const MSetDomain& domain, const InnerDomainPtrType&,
                           int numberValsRequired,
                           std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(1, domain.inner, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "mSetLiftSingle_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<MSetValue>().front());
                if (val.numberElements() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id);
                UInt indexToChange = vioContainerAtThisLevel.selectRandomVar(
                    val.numberElements() - 1);
                HashType oldHash =
                    val.notifyPossibleMemberChange<InnerValueType>(
                        indexToChange);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    auto statusHashPair = val.tryMemberChange<InnerValueType>(
                        indexToChange, oldHash,
                        [&]() { return params.parentCheck(params.vals); });
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
                    changeAccepted, parentCheck, 1, changingMembers,
                    params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryMemberChange<InnerValueType>(indexToChange, oldHash,
                                                        [&]() { return true; });
                }
            });
    }
}

void mSetLiftSingleGen(const MSetDomain& domain, int numberValsRequired,
                       std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            mSetLiftSingleGenImpl(domain, innerDomainPtr, numberValsRequired,
                                  neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void mSetLiftMultipleGenImpl(const MSetDomain& domain, const InnerDomainPtrType,
                             int numberValsRequired,
                             std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(0, domain.inner, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        if (innerNh.numberValsRequired < 2) {
            continue;
        }

        neighbourhoods.emplace_back(
            "mSetLiftMultiple_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)},
             innerNhNumberValsRequired{innerNh.numberValsRequired}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<MSetValue>().front());
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
                std::vector<HashType> oldHashes;
                val.notifyPossibleMembersChange<InnerValueType>(indicesToChange,
                                                                oldHashes);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    return val.tryMembersChange<InnerValueType>(
                        indicesToChange, oldHashes,
                        [&]() { return params.parentCheck(params.vals); });
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
                    changeAccepted, parentCheck, 1, changingMembers,
                    params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryMembersChange<InnerValueType>(
                        indicesToChange, oldHashes, [&]() { return true; });
                }
            });
    }
}

void mSetLiftMultipleGen(const MSetDomain& domain, int numberValsRequired,
                         std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            mSetLiftMultipleGenImpl(domain, innerDomainPtr, numberValsRequired,
                                    neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomain>
struct MSetAdd : public NeighbourhoodFunc<MSetDomain, 1, MSetAdd<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const MSetDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    MSetAdd(const MSetDomain& domain)
        : domain(domain),
          innerDomain(*mpark::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "MSetAdd"; }
    static bool matches(const MSetDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE;
    }
    void apply(NeighbourhoodParams& params, MSetValue& val) {
        if (val.numberElements() == domain.sizeAttr.maxSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        auto newMember = constructValueFromDomain(innerDomain);
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Looking for value to add");
        bool success;
        do {
            assignRandomValueInDomain(innerDomain, *newMember, params.stats);
            success = val.tryAddMember(
                newMember, [&]() { return params.parentCheck(params.vals); });
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
struct MSetRemove
    : public NeighbourhoodFunc<MSetDomain, 1, MSetRemove<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;

    const MSetDomain& domain;
    MSetRemove(const MSetDomain& domain) : domain(domain) {}
    static string getName() { return "MSetRemove"; }
    static bool matches(const MSetDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE;
    }
    void apply(NeighbourhoodParams& params, MSetValue& val) {
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
            val.tryAddMember(std::move(removedMember), []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct MSetAssignRandom
    : public NeighbourhoodFunc<MSetDomain, 1, MSetAssignRandom<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;

    const MSetDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    MSetAssignRandom(const MSetDomain& domain)
        : domain(domain),
          innerDomain(*mpark::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}

    static string getName() { return "MSetAssignRandom"; }
    static bool matches(const MSetDomain&) { return true; }
    void apply(NeighbourhoodParams& params, MSetValue& val) {
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Assigning random value: original value is "
                                   << asView(val));
        auto backup = deepCopy(val);
        backup->container = val.container;
        auto newValue = constructValueFromDomain(domain);
        newValue->container = val.container;
        bool success;
        do {
            assignRandomValueInDomain(domain, *newValue, params.stats);
            success = val.tryAssignNewValue(
                *newValue, [&]() { return params.parentCheck(params.vals); });
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
const AnyDomainRef getInner<MSetDomain>(const MSetDomain& domain) {
    return domain.inner;
}

const NeighbourhoodVec<MSetDomain> NeighbourhoodGenList<MSetDomain>::value = {
    {1, mSetLiftSingleGen},
    {1, mSetLiftMultipleGen},
    {1, generateForAllTypes<MSetDomain, MSetAdd>},    //
    {1, generateForAllTypes<MSetDomain, MSetRemove>}  //
    //    {1, generateForAllTypes<MSetDomain, MSetAssignRandom>}  //
};

const NeighbourhoodVec<MSetDomain>
    NeighbourhoodGenList<MSetDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<MSetDomain>
    NeighbourhoodGenList<MSetDomain>::splitNeighbourhoods = {};
