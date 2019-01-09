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
    UInt numberParts = 0;
    HashType cachedHashTotal = 0;

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType calcMemberPartHash(UInt index) const {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));
        debug_code(assert(index < memberPartMap.size()));
        HashType input[2];
        input[0] = getValueHash(
            members[index]->view().checkedGet(NO_PARTITION_UNDEFINED));
        input[1] = memberPartMap[index];
        return mix(((char*)input), sizeof(input));
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void swapContainingParts(UInt member1Index, UInt member2Index) {
        debug_code(assert(member1Index < memberPartMap.size()));
        debug_code(assert(member2Index < memberPartMap.size()));
        if (memberPartMap[member1Index] == memberPartMap[member2Index]) {
            return;
        }

        cachedHashTotal -= calcMemberPartHash<InnerViewType>(member1Index);
        cachedHashTotal -= calcMemberPartHash<InnerViewType>(member2Index);
        std::swap(memberPartMap[member1Index], memberPartMap[member2Index]);
        cachedHashTotal += calcMemberPartHash<InnerViewType>(member1Index);
        cachedHashTotal += calcMemberPartHash<InnerViewType>(member2Index);
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
        cachedHashTotal = 0;
        memberPartMap.clear();
        hashIndexMap.clear();
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
