#ifndef SRC_TYPES_PARTITION_H_
#define SRC_TYPES_PARTITION_H_
#include <unordered_map>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "triggers/partitionTrigger.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

static const char* NO_PARTITION_UNDEFINED =
    "Not currently supporting partitions with undefined members.\n";
struct PartitionView : public ExprInterface<PartitionView>,
                       public TriggerContainer<PartitionView> {
    friend PartitionValue;
    class PartInfo {
        HashType partHash = HashType(0);

       public:
        UInt partSize = 0;  // number of members

        inline HashType mixedPartHash() {
            return (partHash != HashType(0)) ? mix(partHash) : HashType(0);
        }
        inline bool operator==(const PartInfo& other) const {
            return partHash == other.partHash && partSize == other.partSize;
        }
        friend inline std::ostream& operator<<(std::ostream& os,
                                               const PartInfo& part) {
            return os << "part(partSize=" << part.partSize
                      << ", partHash=" << part.partHash << ")";
        }
        template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
        void notifyMemberAdded(const ExprRef<InnerViewType>& member) {
            ++partSize;
            partHash += mix(getHashForceDefined(member));
        }
        template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
        void notifyMemberRemoved(const ExprRef<InnerViewType>& member) {
            debug_code(assert(partSize > 0));
            --partSize;
            partHash -= mix(getHashForceDefined(member));
        }
        template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
        HashType getHashForceDefined(const ExprRef<InnerViewType>& member) {
            return getValueHash(
                member->getViewIfDefined().checkedGet(NO_PARTITION_UNDEFINED));
        }
    };

    AnyExprVec members;                    // each point in the partition
    HashMap<HashType, UInt> hashIndexMap;  // map from each point's hash to its
                                           // position in members above
    std::vector<PartInfo>
        partInfo;  // info on each part, should be same size as members
    size_t numberParts = 0;           // number non empty parts
    std::vector<UInt> memberPartMap;  // map from each member index to its part
    HashType cachedHashTotal = HashType(0);

    virtual inline AnyExprVec& getChildrenOperands() { shouldNotBeCalledPanic; }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    HashType getMemberHash(UInt index) {
        return getValueHash(
            getMembers<InnerViewType>()[index]->getViewIfDefined().checkedGet(
                NO_PARTITION_UNDEFINED));
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void swapContainingParts(UInt member1Index, UInt member2Index) {
        debug_code(assert(member1Index < memberPartMap.size()));
        debug_code(assert(member2Index < memberPartMap.size()));
        if (memberPartMap[member1Index] == memberPartMap[member2Index]) {
            return;
        }
        UInt part1 = memberPartMap[member1Index];
        UInt part2 = memberPartMap[member2Index];
        auto& member1 = getMembers<InnerViewType>()[member1Index];
        auto& member2 = getMembers<InnerViewType>()[member2Index];
        cachedHashTotal -= partInfo[part1].mixedPartHash();
        cachedHashTotal -= partInfo[part2].mixedPartHash();

        partInfo[part1].notifyMemberRemoved(member1);
        partInfo[part2].notifyMemberRemoved(member2);

        partInfo[part1].notifyMemberAdded(member2);
        partInfo[part2].notifyMemberAdded(member1);

        cachedHashTotal += partInfo[part1].mixedPartHash();
        cachedHashTotal += partInfo[part2].mixedPartHash();
        std::swap(memberPartMap[member1Index], memberPartMap[member2Index]);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    std::vector<UInt> moveMembersToPart(const std::vector<UInt>& memberIndices,
                                        UInt destPart) {
        std::vector<UInt> oldParts;
        for (auto memberIndex : memberIndices) {
            debug_code(assert(memberIndex < memberPartMap.size()));
            UInt part = memberPartMap[memberIndex];
            oldParts.emplace_back(part);
            auto& member = getMembers<InnerViewType>()[memberIndex];
            cachedHashTotal -= partInfo[part].mixedPartHash();
            cachedHashTotal -= partInfo[destPart].mixedPartHash();

            partInfo[part].notifyMemberRemoved(member);
            partInfo[destPart].notifyMemberAdded(member);

            cachedHashTotal += partInfo[part].mixedPartHash();
            cachedHashTotal += partInfo[destPart].mixedPartHash();
            memberPartMap[memberIndex] = destPart;
            if (partInfo[part].partSize == 0) {
                --numberParts;
            }
            if (partInfo[destPart].partSize == 1) {
                ++numberParts;
            }
        }
        return oldParts;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void moveMembersFromPart(const std::vector<UInt>& partMembers,
                             const std::vector<UInt>& memberNewParts) {
        debug_code(assert(partMembers.size() == memberNewParts.size()));
        debug_code(assert(!partMembers.empty()));
        debug_code(assert(partMembers[0] < memberPartMap.size()));
        UInt part = memberPartMap[partMembers[0]];
        for (size_t i = 0; i < partMembers.size(); i++) {
            auto index = partMembers[i];
            auto destPart = memberNewParts[i];
            debug_code(assert(index < memberPartMap.size()));
            debug_code(assert(memberPartMap[index] == part));
            auto member = getMembers<InnerViewType>()[index];
            cachedHashTotal -= partInfo[part].mixedPartHash();
            cachedHashTotal -= partInfo[destPart].mixedPartHash();

            partInfo[part].notifyMemberRemoved(member);
            partInfo[destPart].notifyMemberAdded(member);
            memberPartMap[index] = destPart;

            cachedHashTotal += partInfo[part].mixedPartHash();
            cachedHashTotal += partInfo[destPart].mixedPartHash();
            if (partInfo[part].partSize == 0) {
                --numberParts;
            }
            if (partInfo[destPart].partSize == 1) {
                ++numberParts;
            }
        }
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getMembers() {
        return lib::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getMembers() const {
        return lib::get<ExprRefVec<InnerViewType>>(members);
    }

    void silentClear() {
        cachedHashTotal = HashType(0);
        memberPartMap.clear();
        hashIndexMap.clear();
        partInfo.clear();
        numberParts = 0;
        lib::visit([&](auto& members) { members.clear(); }, members);
    }

    size_t numberElements() const { return memberPartMap.size(); }
    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_PARTITION_H_ */
