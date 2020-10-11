#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/partitionVal.h"
#include "utils/random.h"

using namespace std;

static vector<UInt> makeRegularPartMapping(const PartitionDomain& domain) {
    vector<UInt> partMapping;
    size_t numberParts =
        globalRandom(domain.numberParts.minSize, domain.numberParts.maxSize);
    size_t partSize = domain.numberElements / numberParts;
    for (size_t i = 0; i < numberParts; i++) {
        for (size_t j = 0; j < partSize; j++) {
            partMapping.emplace_back(i);
        }
    }
    shuffle(begin(partMapping), end(partMapping), globalRandomGenerator);
    return partMapping;
}

static vector<UInt> makeRandomPartMapping(const PartitionDomain& domain) {
    vector<UInt> elements(domain.numberElements);
    iota(begin(elements), end(elements), 0);
    shuffle(begin(elements), end(elements), globalRandomGenerator);
    vector<UInt> partSizes(domain.numberElements, 0);
    vector<UInt> partMapping(domain.numberElements);
    size_t numberParts = 0;
    auto insertIntoPart = [&](UInt part) {
        UInt numElementsToInsert =
            (partSizes[part] < domain.partSize.minSize)
                ? domain.partSize.minSize - partSizes[part]
                : 1;
        if (partSizes[part] + numElementsToInsert >
                domain.partSize.maxSize  // no space left in part
            || numElementsToInsert >
                   elements.size()  // not enough elements to fill part min size
        ) {
            return false;
        }
        if (partSizes[part] == 0) {
            // adding to this part creates a new part
            if (numberParts == domain.numberParts.maxSize) {
                // already reached limit to number of parts
                return false;
            }
            numberParts += 1;
        }
        for (size_t i = 0; i < numElementsToInsert; i++) {
            auto element = elements.back();
            elements.pop_back();
            partMapping[element] = part;
            partSizes[part] += 1;
        }
        return true;
    };
    for (size_t part = 0; part < domain.numberParts.minSize; part++) {
        insertIntoPart(part);
    }
    while (!elements.empty()) {
        insertIntoPart(globalRandom<size_t>(0, domain.numberParts.maxSize - 1));
    }

    return partMapping;
}

template <typename InnerDomainPtrType>
bool assignRandomValueInDomainImpl(const PartitionDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   PartitionValue& val,
                                   NeighbourhoodResourceTracker& resource) {
    typedef typename BaseType<InnerDomainPtrType>::element_type InnerDomainType;
    typedef typename AssociatedValueType<InnerDomainType>::type InnerValueType;
    vector<UInt> partMapping;
    if (domain.regular) {
        partMapping = makeRegularPartMapping(domain);
    } else {
        partMapping = makeRandomPartMapping(domain);
    }
    // clear partition and populate with new random elements
    val.silentClear();
    val.setNumberElements<InnerValueType>(domain.numberElements);
    size_t numberElementsInserted = 0;
    while (domain.numberElements > numberElementsInserted) {
        auto reserved = resource.reserve(domain.numberElements, innerDomainPtr,
                                         numberElementsInserted);
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        bool success =
            assignRandomValueInDomain(*innerDomainPtr, *newMember, resource);
        if (!success) {
            return false;
        }
        if (val.assignMember(numberElementsInserted, partMapping.back(),
                             newMember)) {
            partMapping.pop_back();
            ++numberElementsInserted;
        }
        // add member may reject elements, not to worry, while loop will simply
        // continue
    }
    return true;
}

template <>
bool assignRandomValueInDomain<PartitionDomain>(
    const PartitionDomain& domain, PartitionValue& val,
    NeighbourhoodResourceTracker& resource) {
    return lib::visit(
        [&](auto& innerDomainPtr) {
            return assignRandomValueInDomainImpl(domain, innerDomainPtr, val,
                                                 resource);
        },
        domain.inner);
}

template <typename T>
T squared(T v) {
    return v * v;
}
template <typename InnerDomain>
struct PartitionSwapParts
    : public NeighbourhoodFunc<PartitionDomain, 1,
                               PartitionSwapParts<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const PartitionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    PartitionSwapParts(const PartitionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "PartitionSwapParts"; }
    static bool matches(const PartitionDomain& domain) {
        return domain.numberParts.maxSize > 1;
    }
    void apply(NeighbourhoodParams& params, PartitionValue& val) {
        if (val.numberParts() < 2) {
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        // squaring value as need to randomly select two non empty parts.
        debug_neighbourhood_action("Looking for indices to swap");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id);

        bool success;
        UInt index1, index2;
        do {
            ++params.stats.minorNodeCount;

            index1 = vioContainerAtThisLevel.selectRandomVar(
                domain.numberElements - 1);
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
        debug_neighbourhood_action("positions swapped: " << index1 << " and "
                                                         << index2);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.trySwapContainingParts<InnerValueType>(index1, index2,
                                                       []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct PartitionMoveParts
    : public NeighbourhoodFunc<PartitionDomain, 1,
                               PartitionMoveParts<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const PartitionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    PartitionMoveParts(const PartitionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "PartitionMoveParts"; }
    static bool matches(const PartitionDomain& domain) {
        return !domain.regular && domain.numberParts.maxSize > 1 &&
               domain.numberParts.sizeType !=
                   SizeAttr::SizeAttrType::EXACT_SIZE;
    }
    bool isValidMove(const PartitionValue& val, UInt oldPart, UInt newPart,
                     UInt numberParts) {
        bool invalid =
            (val.partInfo[oldPart].partSize == domain.partSize.minSize &&
             domain.partSize.minSize > 1) ||  // old part can't get any smaller
            val.partInfo[newPart].partSize ==
                domain.partSize.maxSize ||  // new part can't get any larger
            (val.partInfo[oldPart].partSize == 1 &&
             numberParts ==
                 domain.numberParts
                     .minSize) ||  // can't delete element from singleton
                                   // part as would reduce number of parts
                                   // which is already at min
            (val.partInfo[newPart].partSize == 0 &&
             numberParts ==
                 domain.numberParts
                     .maxSize) ||  // can't move to new part as would introduce
                                   // a new part when number parts is at max
            (val.partInfo[oldPart].partSize == 1 &&
             val.partInfo[newPart].partSize ==
                 0);  // useless move from one singleton part to another
        return !invalid;
    }

    void apply(NeighbourhoodParams& params, PartitionValue& val) {
        if (val.numberParts() < 2) {
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;

        debug_neighbourhood_action("Looking for index to move");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id);

        bool success;
        UInt index, oldPart, newPart;
        do {
            ++params.stats.minorNodeCount;

            index = vioContainerAtThisLevel.selectRandomVar(
                domain.numberElements - 1);
            oldPart = val.memberPartMap[index];
            newPart = globalRandom<size_t>(1, val.numberElements() - 1);
            // shift newPart not to clash with old part.
            newPart = (oldPart + newPart) % val.numberElements();
            if (!isValidMove(val, oldPart, newPart, val.numberParts())) {
                success = false;
                continue;
            }

            success = val.tryMoveMembersToPart<InnerValueType>(
                             {index}, newPart,
                             [&]() { return params.parentCheck(params.vals); })
                          .has_value();
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find member to move from one part to another, number "
                "tries="
                << tryLimit);
            return;
        }
        debug_neighbourhood_action("element moved parts: index"
                                   << index << " oldPart " << oldPart
                                   << " and newPart " << newPart);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.tryMoveMembersToPart<InnerValueType>({index}, oldPart,
                                                     []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct PartitionMergeParts
    : public NeighbourhoodFunc<PartitionDomain, 1,
                               PartitionMergeParts<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const PartitionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    PartitionMergeParts(const PartitionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "PartitionMergeParts"; }
    static bool matches(const PartitionDomain& domain) {
        return domain.numberParts.maxSize >
                   1 &&  // partition with one part makes no sense here
               (!domain.regular ||
                domain.numberParts.maxSize ==
                    2) &&  // since we are merging parts, either this is not
                           // regular or max number parts is 2
               domain.numberParts.sizeType !=
                   SizeAttr::SizeAttrType::EXACT_SIZE;  // allows variable
                                                        // number of parts
    }
    bool isValidMove(const PartitionValue& val, UInt srcPart, UInt destPart) {
        bool invalid =
            srcPart == destPart ||  // can't merge part with itsself
            val.partInfo[srcPart].partSize == 0 ||   // can't merge empty part
            val.partInfo[destPart].partSize == 0 ||  // can't merge empty parts
            val.partInfo[srcPart].partSize + val.partInfo[destPart].partSize >
                domain.partSize.maxSize;  // dest partwould become to large
        return !invalid;
    }

    void apply(NeighbourhoodParams& params, PartitionValue& val) {
        if (val.numberParts() == domain.partSize.minSize) {
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;

        debug_neighbourhood_action("Looking for parts to merge");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id);

        bool success;
        UInt index, srcPart, destPart;
        std::vector<UInt> memberIndices;
        do {
            ++params.stats.minorNodeCount;

            index = vioContainerAtThisLevel.selectRandomVar(
                domain.numberElements - 1);
            srcPart = val.memberPartMap[index];
            destPart = val.parts.randomElement();
            if (!isValidMove(val, srcPart, destPart)) {
                success = false;
                continue;
            }

            memberIndices = val.getMembersInPart(srcPart);

            success = val.tryMoveMembersToPart<InnerValueType>(
                             memberIndices, destPart,
                             [&]() { return params.parentCheck(params.vals); })
                          .has_value();
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find parts to merge, number "
                "tries="
                << tryLimit);
            return;
        }
        debug_neighbourhood_action("Parts merged: srcPart "
                                   << srcPart << " and destPart " << destPart
                                   << " and indices " << memberIndices);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.tryMoveMembersToPart<InnerValueType>(memberIndices, srcPart,
                                                     []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct PartitionSplitParts
    : public NeighbourhoodFunc<PartitionDomain, 1,
                               PartitionSplitParts<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const PartitionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    PartitionSplitParts(const PartitionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "PartitionSplitParts"; }
    static bool matches(const PartitionDomain& domain) {
        return domain.numberParts.maxSize >
                   1 &&  // partition with one part makes no sense here
               (!domain.regular ||
                domain.numberParts.maxSize ==
                    2) &&  // since we are splitting parts, either this is not
                           // regular or max number parts is 2
               domain.numberParts.sizeType !=
                   SizeAttr::SizeAttrType::EXACT_SIZE;  // allows variable
                                                        // number of parts
    }
    bool isValidMove(const PartitionValue& val, UInt srcPart, UInt destPart) {
        bool invalid =
            srcPart == destPart ||  // can't split part with itsself
            val.partInfo[srcPart].partSize == 0 ||  // can't Split empty part
            val.partInfo[destPart].partSize !=
                0 ||  // can't split into existing part, destPart must be empty
            val.partInfo[srcPart].partSize - domain.partSize.minSize <
                domain.partSize.minSize;  // spliting this part would leave
                                          // srcPart too small
        return !invalid;
    }
    void apply(NeighbourhoodParams& params, PartitionValue& val) {
        if (val.numberParts() == domain.partSize.maxSize) {
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;

        debug_neighbourhood_action("Looking for parts to Split");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id);

        bool success;
        UInt index, srcPart, destPart;
        std::vector<UInt> memberIndices;
        do {
            ++params.stats.minorNodeCount;

            index = vioContainerAtThisLevel.selectRandomVar(
                domain.numberElements - 1);
            srcPart = val.memberPartMap[index];
            destPart = val.emptyParts.randomElement();
            if (!isValidMove(val, srcPart, destPart)) {
                success = false;
                continue;
            }
            // work out the max number of elements that can be moved from src to
            // dest, to be used in random generator
            const UInt minSplitSize = domain.partSize.minSize;
            const UInt maxSplitSize = min<UInt>(
                val.partInfo[srcPart].partSize - domain.partSize.minSize,
                domain.partSize.maxSize);
            UInt splitSize = globalRandom(minSplitSize, maxSplitSize);

            memberIndices = val.getMembersInPart(srcPart);
            // randomly select splitSize members from memberIndices
            shuffle(begin(memberIndices), end(memberIndices),
                    globalRandomGenerator);
            memberIndices.resize(splitSize);

            success = val.tryMoveMembersToPart<InnerValueType>(
                             memberIndices, destPart,
                             [&]() { return params.parentCheck(params.vals); })
                          .has_value();
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find parts to split, number "
                "tries="
                << tryLimit);
            return;
        }
        debug_neighbourhood_action("Parts splitd: srcPart "
                                   << srcPart << " and destPart " << destPart
                                   << " and indices " << memberIndices);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.tryMoveMembersToPart<InnerValueType>(memberIndices, srcPart,
                                                     []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct PartitionRemoveAndDistributePart
    : public NeighbourhoodFunc<PartitionDomain, 1,
                               PartitionRemoveAndDistributePart<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const PartitionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    PartitionRemoveAndDistributePart(const PartitionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "PartitionRemoveAndDistributePart"; }
    static bool matches(const PartitionDomain& domain) {
        return domain.numberParts.maxSize >
                   1 &&  // partition with one part makes no sense here
               !domain.regular &&
               domain.numberParts.sizeType !=
                   SizeAttr::SizeAttrType::EXACT_SIZE;  // allows variable
                                                        // number of parts
    }
    lib::optional<vector<UInt>> findDestParts(const PartitionValue& val,
                                              UInt srcPart) {
        vector<UInt> destParts;
        HashMap<UInt, UInt> partExtraMemberCount;
        auto newPartSize = [&](UInt p) {
            auto iter = partExtraMemberCount.find(p);
            UInt extra =
                (iter != partExtraMemberCount.end()) ? iter->second : 0;
            return extra + val.partInfo[p].partSize;
        };

        auto iter = val.parts.begin();
        bool inserted = false;
        while (destParts.size() < val.partInfo[srcPart].partSize) {
            if (iter == val.parts.end()) {
                if (!inserted) {
                    // we have done one rotation of parts and could not insert
                    // anything
                    return lib::nullopt;
                }
                iter = val.parts.begin();
                inserted = false;
            }
            if (*iter != (Int)srcPart &&
                newPartSize(*iter) < (UInt)domain.partSize.maxSize) {
                inserted = true;
                destParts.emplace_back(*iter);
                partExtraMemberCount[*iter] += 1;
            }
            ++iter;
        }
        shuffle(destParts.begin(), destParts.end(), globalRandomGenerator);
        return destParts;
    }
    void apply(NeighbourhoodParams& params, PartitionValue& val) {
        if (val.numberParts() == domain.partSize.minSize) {
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;

        debug_neighbourhood_action(
            "Looking for part to remove and redistribute");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id);

        bool success = false;
        UInt index, srcPart;
        std::vector<UInt> memberIndices;
        lib::optional<std::vector<UInt>> destParts;

        do {
            success = false;
            ++params.stats.minorNodeCount;

            index = vioContainerAtThisLevel.selectRandomVar(
                domain.numberElements - 1);
            srcPart = val.memberPartMap[index];
            destParts = findDestParts(val, srcPart);
            if (!destParts) {
                success = false;
                continue;
            }
            memberIndices = val.getMembersInPart(srcPart);

            success = val.tryMoveMembersFromPart<InnerValueType>(
                memberIndices, *destParts,
                [&]() { return params.parentCheck(params.vals); });
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find part to redistribute, number "
                "tries="
                << tryLimit);
            return;
        }
        debug_neighbourhood_action("part redistributed:  srcPart="
                                   << srcPart << " and destParts " << *destParts
                                   << " and indices " << memberIndices);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.tryMoveMembersToPart<InnerValueType>(memberIndices, srcPart,
                                                     []() { return true; });
        }
    }
};

template <typename InnerDomain>
struct PartitionCollectElementsIntoNewPart
    : public NeighbourhoodFunc<
          PartitionDomain, 1,
          PartitionCollectElementsIntoNewPart<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const PartitionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    PartitionCollectElementsIntoNewPart(const PartitionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.inner)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "PartitionCollectElementsIntoNewPart"; }
    static bool matches(const PartitionDomain& domain) {
        return domain.numberParts.maxSize >
                   1 &&  // partition with one part makes no sense here
               !domain.regular &&
               domain.numberParts.sizeType !=
                   SizeAttr::SizeAttrType::EXACT_SIZE;  // allows variable
                                                        // number of parts
    }
    lib::optional<vector<UInt>> findMemberIndices(
        const PartitionValue& val,
        ViolationContainer& vioContainerAtThisLevel) {
        HashMap<UInt, UInt> deletionsFromPart;
        HashSet<UInt> memberIndicesSet;
        auto newPartSize = [&](UInt p) {
            auto iter = deletionsFromPart.find(p);
            UInt deletions =
                (iter != deletionsFromPart.end()) ? iter->second : 0;
            return val.partInfo[p].partSize - deletions;
        };

        UInt targetNumberElements =
            min(val.numberParts(), domain.partSize.maxSize);
        targetNumberElements =
            max((size_t)targetNumberElements, domain.partSize.maxSize);

        UInt64 failedAttempts = 0;
        while (memberIndicesSet.size() < targetNumberElements &&
               failedAttempts < domain.numberElements * 20) {
            bool allowedDuplicateParts =
                failedAttempts < domain.numberElements * 10;

            auto index = vioContainerAtThisLevel.selectRandomVar(
                domain.numberElements - 1);
            UInt part = val.memberPartMap[index];
            if (domain.partSize.minSize + 1 <=
                    newPartSize(part) &&  // deleting from this part does not
                                          // violate minSize
                (!deletionsFromPart.count(part) ||
                 allowedDuplicateParts)  // aim for non duplicate parts in early
                                         // attempts
            ) {
                deletionsFromPart[part] += 1;
                memberIndicesSet.insert(index);
            } else {
                ++failedAttempts;
            }
        }
        if (memberIndicesSet.size() >= domain.partSize.minSize) {
            return vector<UInt>(memberIndicesSet.begin(),
                                memberIndicesSet.end());
        } else {
            return lib::nullopt;
        }
    }
    void apply(NeighbourhoodParams& params, PartitionValue& val) {
        if (val.numberParts() == domain.partSize.maxSize) {
            return;
        }
        debug_code(assert(!val.emptyParts.empty()));

        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;

        debug_neighbourhood_action(
            "Looking for elements to collect into new part");

        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id);

        bool success = false;
        UInt destPart = *val.emptyParts.begin();
        lib::optional<vector<UInt>> memberIndices;
        lib::optional<vector<UInt>> oldParts;
        do {
            success = false;
            ++params.stats.minorNodeCount;

            memberIndices = findMemberIndices(val, vioContainerAtThisLevel);
            if (!memberIndices) {
                success = false;
                continue;
            }
            oldParts = val.tryMoveMembersToPart<InnerValueType>(
                *memberIndices, destPart,
                [&]() { return params.parentCheck(params.vals); });
            success = oldParts.has_value();
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find elements to collect, number "
                "tries="
                << tryLimit);
            return;
        }
        debug_neighbourhood_action(
            "Elements collected into new part, memberIndices="
            << *memberIndices << " and destPart= " << destPart
            << " and oldParts=" << *oldParts);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.tryMoveMembersFromPart<InnerValueType>(
                *memberIndices, *oldParts, []() { return true; });
        }
    }
};

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
            NeighbourhoodResourceAllocator allocator(domain);
            do {
                auto resource = allocator.requestLargerResource();
                success =
                    assignRandomValueInDomain(domain, *newValue, resource);
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
        });
}

const NeighbourhoodVec<PartitionDomain> NeighbourhoodGenList<
    PartitionDomain>::value = {
    {1, generateForAllTypes<PartitionDomain, PartitionSwapParts>},
    {1, generateForAllTypes<PartitionDomain, PartitionMoveParts>},
    {1, generateForAllTypes<PartitionDomain, PartitionMergeParts>},
    {1, generateForAllTypes<PartitionDomain, PartitionSplitParts>},
    {1, generateForAllTypes<PartitionDomain, PartitionRemoveAndDistributePart>},
    {1,
     generateForAllTypes<PartitionDomain, PartitionCollectElementsIntoNewPart>},
};

const NeighbourhoodVec<PartitionDomain>
    NeighbourhoodGenList<PartitionDomain>::mergeNeighbourhoods = {};

const NeighbourhoodVec<PartitionDomain>
    NeighbourhoodGenList<PartitionDomain>::splitNeighbourhoods = {};
