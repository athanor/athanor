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
    AnyExprVec members;
    std::vector<UInt> memberPartMap;
    std::unordered_map<HashType, UInt> hashIndexMap;
    static const HashType INITIAL_HASH;
    std::vector<HashType> partHashes;
    HashType cachedHashTotal = INITIAL_HASH;

    size_t numberParts() const { return partHashes.size(); }

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
        HashType member1Hash = getMemberHash<InnerViewType>(member1Index);
        HashType member2Hash = getMemberHash<InnerViewType>(member2Index);

        cachedHashTotal -= mix(partHashes[part1]);
        cachedHashTotal -= mix(partHashes[part2]);
        partHashes[part1] -= mix(member1Hash);
        partHashes[part2] -= mix(member2Hash);
        partHashes[part1] += mix(member2Hash);
        partHashes[part2] += mix(member1Hash);
        cachedHashTotal += mix(partHashes[part1]);
        cachedHashTotal += mix(partHashes[part2]);
        std::swap(memberPartMap[member1Index], memberPartMap[member2Index]);
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getMembers() {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getMembers() const {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    void silentClear() {
        cachedHashTotal = INITIAL_HASH;
        memberPartMap.clear();
        hashIndexMap.clear();
        partHashes.clear();
        mpark::visit([&](auto& members) { members.clear(); }, members);
    }

    inline void notifyEntirePartitionChange() {
        debug_code(standardSanityChecksForThisType());
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    size_t numberElements() { return memberPartMap.size(); }
    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_PARTITION_H_ */
