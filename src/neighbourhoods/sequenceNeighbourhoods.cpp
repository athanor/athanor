#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/sequenceVal.h"
#include "utils/random.h"

using namespace std;

template <typename InnerDomainPtrType>
bool assignRandomValueInDomainImpl(const SequenceDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   SequenceValue& val,
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
    // clear sequence and populate with new random elements
    val.silentClear();
    while (newNumberElements > val.numberElements()) {
        auto reserved = resource.reserve(domain.sizeAttr.minSize,
                                         innerDomainPtr, val.numberElements());
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        bool success =
            assignRandomValueInDomain(*innerDomainPtr, *newMember, resource);
        if (!success) {
            return val.numberElements() >= domain.sizeAttr.minSize;
        }
        val.addMember(val.numberElements(), newMember);
        // add member may reject elements, not to worry, while loop will simply
        // continue
    }
    return true;
}

template <>
bool assignRandomValueInDomain<SequenceDomain>(
    const SequenceDomain& domain, SequenceValue& val,
    NeighbourhoodResourceTracker& resource) {
    return lib::visit(
        [&](auto& innerDomainPtr) {
            return assignRandomValueInDomainImpl(domain, innerDomainPtr, val,
                                                 resource);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void sequenceLiftSingleGenImpl(const SequenceDomain& domain,
                               const InnerDomainPtrType&,
                               int numberValsRequired,
                               std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(1, domain.inner, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    UInt innerDomainSize = getDomainSize(domain.inner);
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "SequenceLiftSingle_" + innerNh.name, numberValsRequired,
            [innerDomainSize, innerNhApply{std::move(innerNh.apply)}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<SequenceValue>().front());
                if (val.numberElements() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id);
                UInt indexToChange = vioContainerAtThisLevel.selectRandomVar(
                    val.numberElements() - 1);
                HashType previousSubsequenceHash;
                std::vector<HashType> subsequenceHashes;
                previousSubsequenceHash =
                    val.notifyPossibleSubsequenceChange<InnerValueType>(
                        indexToChange, indexToChange + 1, subsequenceHashes);
                ParentCheckCallBack parentCheck =
                    [&](const AnyValVec& newValue) {
                        if (val.injective) {
                            if (val.containsMember(
                                    lib::get<ValRefVec<InnerValueType>>(
                                        newValue)
                                        .front())) {
                                return false;
                            }
                        }
                        return val.trySubsequenceChange<InnerValueType>(
                            indexToChange, indexToChange + 1, subsequenceHashes,
                            previousSubsequenceHash,
                            [&]() { return params.parentCheck(params.vals); });
                    };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        previousSubsequenceHash =
                            val.notifyPossibleSubsequenceChange<InnerValueType>(
                                indexToChange, indexToChange + 1,
                                subsequenceHashes);
                    }
                    return !requiresRevert;
                };
                AnyValVec changingMembers;
                auto& changingMembersImpl =
                    changingMembers.emplace<ValRefVec<InnerValueType>>();
                changingMembersImpl.emplace_back(
                    val.member<InnerValueType>(indexToChange));
                int tryLimit = (val.injective)
                                   ? calcNumberInsertionAttempts(
                                         val.numberElements(), innerDomainSize)
                                   : 1;
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck,
                    tryLimit * params.parentCheckTryLimit, changingMembers,
                    params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.trySubsequenceChange<InnerValueType>(
                        indexToChange, indexToChange + 1, subsequenceHashes,
                        previousSubsequenceHash, [&]() { return true; });
                }
            });
    }
}

void sequenceLiftSingleGen(const SequenceDomain& domain, int numberValsRequired,
                           std::vector<Neighbourhood>& neighbourhoods) {
    lib::visit(
        [&](const auto& innerDomainPtr) {
            sequenceLiftSingleGenImpl(domain, innerDomainPtr,
                                      numberValsRequired, neighbourhoods);
        },
        domain.inner);
}
template <typename InnerDomain>
struct SequenceAdd
    : public NeighbourhoodFunc<SequenceDomain, 1, SequenceAdd<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SequenceDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SequenceAdd(const SequenceDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SequenceAdd"; }
    static bool matches(const SequenceDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE;
    }
    void apply(NeighbourhoodParams& params, SequenceValue& val) {
        if (val.numberElements() == domain.sizeAttr.maxSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        auto newMember = constructValueFromDomain(innerDomain);
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Looking for value to add");
        bool success;
        size_t indexOfNewMember;
        NeighbourhoodResourceAllocator allocator(innerDomain);
        do {
            auto resource = allocator.requestLargerResource();
            success =
                assignRandomValueInDomain(innerDomain, *newMember, resource);
            params.stats.minorNodeCount += resource.getResourceConsumed();
            indexOfNewMember = globalRandom<UInt>(0, val.numberElements());
            success =
                success && val.tryAddMember(indexOfNewMember, newMember, [&]() {
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
            val.tryRemoveMember<InnerValueType>(indexOfNewMember,
                                                []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SequenceRemove
    : public NeighbourhoodFunc<SequenceDomain, 1, SequenceRemove<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;

    const SequenceDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SequenceRemove(const SequenceDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SequenceRemove"; }
    static bool matches(const SequenceDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE;
    }
    void apply(NeighbourhoodParams& params, SequenceValue& val) {
        if (val.numberElements() == domain.sizeAttr.minSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        size_t indexToRemove;
        int numberTries = 0;
        ValRef<InnerValueType> removedMember(nullptr);
        bool success;
        debug_neighbourhood_action("Looking for value to remove");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id);
        do {
            ++params.stats.minorNodeCount;
            indexToRemove = vioContainerAtThisLevel.selectRandomVar(
                                                                    val.numberElements() - 1);;
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
            val.tryAddMember(indexToRemove, std::move(removedMember),
                             []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SequenceRelaxSub
    : public NeighbourhoodFunc<SequenceDomain, 1,
                               SequenceRelaxSub<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;

    const SequenceDomain& domain;
    shared_ptr<InnerDomain> innerDomainPtr;
    const InnerDomain& innerDomain;

    const UInt innerDomainSize;
    SequenceRelaxSub(const SequenceDomain& domain)
        : domain(domain),
          innerDomainPtr(lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SequenceRelaxSub"; }
    static bool matches(const SequenceDomain&) { return true; }

    template <typename InnerDomainType>
    static void insureSize(
        const InnerDomainType& innerDomain,
        ExprRefVec<typename AssociatedViewType<InnerDomainType>::type>&
            newValues,
        size_t subseqSize) {
        if (subseqSize < newValues.size()) {
            newValues.erase(newValues.begin() + subseqSize, newValues.end());
        }
        while (newValues.size() < subseqSize) {
            auto newMember = constructValueFromDomain(innerDomain);
            newValues.emplace_back(newMember.asExpr());
        }
    }

    template <typename InnerDomainType, typename InnerValueType,
              typename InnerViewType>
    bool assignNewValues(InnerDomainType& innerDomain, InnerValueType& val,
                         vector<HashType>& oldHashes,
                         ExprRefVec<InnerViewType>& newValues,
                         NeighbourhoodResourceTracker& resource) {
        if (val.injective) {
            // temperarily remove old hashes from val, these will be restored.
            for (auto& hash : oldHashes) {
                val.memberHashes.erase(hash);
            }
        }

        vector<HashType> newHashes;  // remember the new hashes that we add to
                                     // val so that we can remove them again.
        bool success = true;
        for (auto& expr : newValues) {
            while (true) {
                success = assignRandomValueInDomain(
                    innerDomain, *assumeAsValue(expr), resource);
                if (!success) {
                    // no more resource, fail
                    break;
                }
                if (!val.injective) {
                    break;
                }
                HashType hash = getValueHash(expr);
                if (!val.memberHashes.count(hash)) {
                    val.memberHashes.insert(hash);
                    newHashes.emplace_back(hash);
                    break;
                }
            }
            if (!success) {
                // if inner while loop exited because of no more resource:
                break;
            }
        }
        if (val.injective) {
            // remove new hashes and restore old hashes
            for (auto& hash : newHashes) {
                val.memberHashes.erase(hash);
            }
            for (auto& hash : oldHashes) {
                val.memberHashes.insert(hash);
            }
        }
        return success;
    }

    template <typename InnerViewType>
    void swapSub(ExprRefVec<InnerViewType>& members,
                 ExprRefVec<InnerViewType>& newValues, UInt startIndex,
                 UInt endIndex) {
        UInt subseqSize = endIndex - startIndex;
        for (size_t i = 0; i < subseqSize; i++) {
            swap(members[startIndex + i], newValues[i]);
        }
    }

    template <typename InnerViewType>
    void swapAssignedValuesOfSub(ExprRefVec<InnerViewType>& members,
                                 ExprRefVec<InnerViewType>& newValues,
                                 UInt startIndex, UInt endIndex) {
        UInt subseqSize = endIndex - startIndex;
        for (size_t i = 0; i < subseqSize; i++) {
            swapValAssignments(*assumeAsValue(newValues[i]),
                               *assumeAsValue(members[startIndex + i]));
        }
    }

    void apply(NeighbourhoodParams& params, SequenceValue& val) {
        if (val.numberElements() < 2) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action(
            "Looking for indices of subsequence to relax");

        bool success;
        UInt startIndex, endIndex;
        ExprRefVec<InnerViewType> newValues;
        vector<HashType> oldHashes;
        auto& members = val.getMembers<InnerViewType>();
        NeighbourhoodResourceAllocator allocator(innerDomain);
        do {
            auto resource = allocator.requestLargerResource();
            startIndex = globalRandom<UInt>(0, val.numberElements() - 1);
            auto subsetSize = resource.randomNumberElements(
                0, val.numberElements() - startIndex, innerDomainPtr);
            if (!subsetSize) {
                success = false;
                continue;
            }
            endIndex =
                min<UInt>(startIndex + *subsetSize, val.numberElements());
            oldHashes.clear();
            HashType previousSubseqHash =
                val.notifyPossibleSubsequenceChange<InnerValueType>(
                    startIndex, endIndex, oldHashes);
            insureSize(innerDomain, newValues, endIndex - startIndex);

            success = assignNewValues(innerDomain, val, oldHashes, newValues,
                                      resource);
            params.stats.minorNodeCount += resource.getResourceConsumed();
            if (!success) {
                continue;
            }
            // temperarily swap values in, and if parent type accepts it,
            // swap it back and do a propper deepCopy
            swapSub(members, newValues, startIndex, endIndex);

            success = val.trySubsequenceChange<InnerValueType>(
                startIndex, endIndex, oldHashes, previousSubseqHash, [&]() {
                    // swap it back
                    swapSub(members, newValues, startIndex, endIndex);
                    if (params.parentCheck(params.vals)) {
                        // swap values of the variables not the variables
                        // and trigger change
                        swapAssignedValuesOfSub(members, newValues, startIndex,
                                                endIndex);
                        return true;
                    }
                    return false;
                });
        } while (!success && ++numberTries < tryLimit);

        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find new values for relaxed subsequence, number "
                "tries="
                << tryLimit);
            return;
        }
        debug_neighbourhood_action(
            "subsequence relaxd: " << startIndex << " and " << endIndex);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            oldHashes.clear();
            HashType previousSubseqHash =
                val.notifyPossibleSubsequenceChange<InnerValueType>(
                    startIndex, endIndex, oldHashes);
            swapAssignedValuesOfSub(members, newValues, startIndex, endIndex);
            val.trySubsequenceChange<InnerValueType>(
                startIndex, endIndex, oldHashes, previousSubseqHash,
                []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SequenceReverseSub
    : public NeighbourhoodFunc<SequenceDomain, 1,
                               SequenceReverseSub<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SequenceDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SequenceReverseSub(const SequenceDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SequenceReverseSub"; }
    static bool matches(const SequenceDomain&) { return true; }
    void apply(NeighbourhoodParams& params, SequenceValue& val) {
        if (val.numberElements() < 2) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action(
            "Looking for indices of subsequence to reverse");

        bool success;
        UInt index1, index2;
        do {
            index1 = globalRandom<UInt>(0, val.numberElements() - 2);
            index2 = globalRandom<UInt>(index1 + 1, val.numberElements() - 1);
            params.stats.minorNodeCount +=
                (index2 - index1) / 2 + ((index2 - index1) % 2 != 0);
            success = val.trySubsequenceReverse<InnerValueType>(
                index1, index2,
                [&]() { return params.parentCheck(params.vals); });
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find subsequence to reverse, number tries="
                << tryLimit);
            return;
        }
        debug_neighbourhood_action("subsequence reversed: " << index1 << " and "
                                                            << index2);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.trySubsequenceReverse<InnerValueType>(index1, index2,
                                                      []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SequenceShuffleSub
    : public NeighbourhoodFunc<SequenceDomain, 1,
                               SequenceShuffleSub<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SequenceDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SequenceShuffleSub(const SequenceDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SequenceShuffleSub"; }
    static bool matches(const SequenceDomain&) { return true; }
    void generateSwap(UInt size, vector<UInt>& swaps) {
        swaps.resize(size);
        for (size_t i = 0; i < swaps.size(); i++) {
            swaps[i] = globalRandom<UInt>(i, swaps.size() - 1);
        }
    }

    void apply(NeighbourhoodParams& params, SequenceValue& val) {
        if (val.numberElements() < 2) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action(
            "Looking for indices of subsequence to Shuffle");

        bool success;
        UInt index1, index2;
        vector<UInt> swaps;
        do {
            index1 = globalRandom<UInt>(0, val.numberElements() - 2);
            index2 = globalRandom<UInt>(index1 + 1, val.numberElements() - 1);
            generateSwap((index2 - index1) + 1, swaps);

            params.stats.minorNodeCount += (index2 - index1) + 1;
            success = val.trySubsequenceShuffle<InnerValueType>(
                index1, swaps,
                [&]() { return params.parentCheck(params.vals); });
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find subsequence to Shuffle, number tries="
                << tryLimit);
            return;
        }
        debug_neighbourhood_action("subsequence Shuffled: " << index1 << " and "
                                                            << index2);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.trySubsequenceShuffle<InnerValueType>(index1, swaps,
                                                      []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SequencePositionsSwap
    : public NeighbourhoodFunc<SequenceDomain, 1,
                               SequencePositionsSwap<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SequenceDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SequencePositionsSwap(const SequenceDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SequencePositionsSwap"; }
    static bool matches(const SequenceDomain&) { return true; }
    void apply(NeighbourhoodParams& params, SequenceValue& val) {
        if (val.numberElements() < 2) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Looking for indices to swap");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id);

        bool success;
        UInt index1, index2;
        do {
            ++params.stats.minorNodeCount;
            index1 = vioContainerAtThisLevel.selectRandomVar(
                val.numberElements() - 1);
            index2 = globalRandom<UInt>(0, val.numberElements() - 1);
            index2 = (index1 + index2) % val.numberElements();
            if (index2 < index1) {
                swap(index1, index2);
            }

            success = val.trySwapPositions<InnerValueType>(
                index1, index2,
                [&]() { return params.parentCheck(params.vals); });
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find positions to swap, number tries=" << tryLimit);
            return;
        }
        debug_neighbourhood_action("positions swapped: " << index1 << " and "
                                                         << index2);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.trySwapPositions<InnerValueType>(index1, index2,
                                                 []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SequenceMove
    : public NeighbourhoodFunc<SequenceDomain, 2, SequenceMove<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SequenceDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SequenceMove(const SequenceDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SequenceMove"; }
    static bool matches(const SequenceDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE;
    }
    void apply(NeighbourhoodParams& params, SequenceValue& fromVal,
               SequenceValue& toVal) {
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
        UInt indexToMove, destIndex;
        do {
            ++params.stats.minorNodeCount;
            indexToMove = globalRandom<UInt>(0, fromVal.numberElements() - 1);
            destIndex = globalRandom<UInt>(0, toVal.numberElements());
            auto memberToMove =
                assumeAsValue(fromVal.getMembers<InnerViewType>()[indexToMove]);
            success = toVal.tryAddMember(destIndex, memberToMove, [&]() {
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
            "Moved value: " << toVal.getMembers<InnerViewType>()[destIndex]);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            auto member = toVal
                              .tryRemoveMember<InnerValueType>(
                                  destIndex, []() { return true; })
                              .second;
            fromVal.tryAddMember(indexToMove, member, [&]() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SequenceCrossover
    : public NeighbourhoodFunc<SequenceDomain, 2,
                               SequenceCrossover<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const SequenceDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SequenceCrossover(const SequenceDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "SequenceCrossover"; }
    static bool matches(const SequenceDomain&) { return true; }

    template <typename InnerValueType, typename Func>
    static bool performCrossOver(SequenceValue& fromVal, SequenceValue& toVal,
                                 UInt indexToCrossOver,
                                 std::vector<HashType>& fromValMemberHashes,
                                 std::vector<HashType>& toValMemberHashes,
                                 ValRef<InnerValueType> member1,
                                 ValRef<InnerValueType> member2,
                                 Func&& parentCheck) {
        fromValMemberHashes.clear();
        toValMemberHashes.clear();
        HashType fromValSubseqHash =
            fromVal.notifyPossibleSubsequenceChange<InnerValueType>(
                indexToCrossOver, indexToCrossOver + 1, fromValMemberHashes);
        HashType toValSubseqHash =
            toVal.notifyPossibleSubsequenceChange<InnerValueType>(
                indexToCrossOver, indexToCrossOver + 1, toValMemberHashes);
        swapValAssignments(*member1, *member2);
        bool success = fromVal.trySubsequenceChange<InnerValueType>(
            indexToCrossOver, indexToCrossOver + 1, fromValMemberHashes,
            fromValSubseqHash, [&]() {
                return toVal.trySubsequenceChange<InnerValueType>(
                    indexToCrossOver, indexToCrossOver + 1, toValMemberHashes,
                    toValSubseqHash, [&]() {
                        if (parentCheck()) {
                            return true;
                        } else {
                            swapValAssignments(*member1, *member2);
                            return false;
                        }
                    });
            });
        return success;
    }

    void apply(NeighbourhoodParams& params, SequenceValue& fromVal,
               SequenceValue& toVal) {
        if (fromVal.numberElements() == 0 || toVal.numberElements() == 0) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit =
            (domain.injective)
                ? params.parentCheckTryLimit *
                      calcNumberInsertionAttempts(fromVal.numberElements(),
                                                  innerDomainSize) *
                      calcNumberInsertionAttempts(toVal.numberElements(),
                                                  innerDomainSize)
                : params.parentCheckTryLimit;
        debug_neighbourhood_action("Looking for values to cross over");
        bool success = false;
        UInt indexToCrossOver;
        const UInt maxCrossOverIndex =
            min(fromVal.numberElements(), toVal.numberElements()) - 1;
        ValRef<InnerValueType> member1 = nullptr, member2 = nullptr;
        std::vector<HashType> fromValMemberHashes, toValMemberHashes;

        do {
            ++params.stats.minorNodeCount;
            indexToCrossOver = globalRandom<UInt>(0, maxCrossOverIndex);
            member1 = fromVal.member<InnerValueType>(indexToCrossOver);
            member2 = toVal.member<InnerValueType>(indexToCrossOver);
            if (domain.injective && (toVal.containsMember(member1) ||
                                     fromVal.containsMember(member2))) {
                continue;
            }
            success = performCrossOver(
                fromVal, toVal, indexToCrossOver, fromValMemberHashes,
                toValMemberHashes, member1, member2,
                [&]() { return params.parentCheck(params.vals); });
        } while (!success && ++numberTries < tryLimit);

        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find values to cross over, number tries="
                << tryLimit);
            return;
        }

        debug_neighbourhood_action(
            "CrossOverd values: "
            << toVal.getMembers<InnerViewType>()[indexToCrossOver] << " and "
            << fromVal.getMembers<InnerViewType>()[indexToCrossOver]);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            performCrossOver(fromVal, toVal, indexToCrossOver,
                             fromValMemberHashes, toValMemberHashes, member1,
                             member2, [&]() { return true; });
        }
    }
};

template <typename InnerDomain>
struct SequenceAssignRandom
    : public NeighbourhoodFunc<SequenceDomain, 1,
                               SequenceAssignRandom<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;

    const SequenceDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    SequenceAssignRandom(const SequenceDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}

    static string getName() { return "SequenceAssignRandom"; }
    static bool matches(const SequenceDomain&) { return true; }
    void apply(NeighbourhoodParams& params, SequenceValue& val) {
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Assigning random value: original value is "
                                   << asView(val));
        auto backup = deepCopy(val);
        backup->container = val.container;
        auto newValue = constructValueFromDomain(domain);
        newValue->container = val.container;
        bool success;
        NeighbourhoodResourceAllocator allocator(domain);
        do {
            auto resource = allocator.requestLargerResource();
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

const NeighbourhoodVec<SequenceDomain>
    NeighbourhoodGenList<SequenceDomain>::value = {
        {1, sequenceLiftSingleGen},
        {1, generateForAllTypes<SequenceDomain, SequenceAdd>},
        {1, generateForAllTypes<SequenceDomain, SequenceRemove>},
        {1, generateForAllTypes<SequenceDomain, SequencePositionsSwap>},
        {1, generateForAllTypes<SequenceDomain, SequenceReverseSub>},
        //{1, generateForAllTypes<SequenceDomain, SequenceShuffleSub>},
        {1, generateForAllTypes<SequenceDomain, SequenceRelaxSub>},
        {2, generateForAllTypes<SequenceDomain, SequenceMove>},
        {2, generateForAllTypes<SequenceDomain, SequenceCrossover>},

};

const NeighbourhoodVec<SequenceDomain>
    NeighbourhoodGenList<SequenceDomain>::mergeNeighbourhoods = {};

const NeighbourhoodVec<SequenceDomain>
    NeighbourhoodGenList<SequenceDomain>::splitNeighbourhoods = {};
