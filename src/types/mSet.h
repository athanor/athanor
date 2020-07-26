#ifndef SRC_TYPES_MSET_H_
#define SRC_TYPES_MSET_H_
#include <algorithm>
#include <unordered_map>
#include <vector>

#include "base/base.h"
#include "common/common.h"
#include "triggers/mSetTrigger.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

static const char* NO_MSET_UNDEFINED_MEMBERS =
    "Not yet handling multisets with undefined members.\n";
struct MSetView : public ExprInterface<MSetView>,
                  public TriggerContainer<MSetView> {
    friend MSetValue;
    AnyExprVec members;
    HashType cachedHashTotal = HashType(0);
    HashMap<HashType, UInt> memberCounts;
    std::vector<HashType> indexHashMap;
    MSetView() {}
    MSetView(AnyExprVec members) : members(std::move(members)) {}

   public:
    void addHash(HashType hash) {
        memberCounts[hash] += 1;
        cachedHashTotal += mix(hash);
    }
    void removeHash(HashType hash) {
        auto countIter = memberCounts.find(hash);
        debug_code(assert(countIter != memberCounts.end()));
        countIter->second -= 1;
        if (countIter->second == 0) {
            memberCounts.erase(countIter);
        }
        cachedHashTotal -= mix(hash);
    }

    UInt memberCount(HashType hash) {
        auto iter = memberCounts.find(hash);
        if (iter != memberCounts.end()) {
            return iter->second;
        }
        return 0;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMember(const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        members.emplace_back(member);
        HashType hash =
            getValueHash(member->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
        indexHashMap.emplace_back(hash);
        addHash(hash);
        debug_code(standardSanityChecksForThisType());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));
        HashType hash = indexHashMap[index];
        indexHashMap[index] = indexHashMap.back();
        ;
        indexHashMap.pop_back();
        ExprRef<InnerViewType> removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        removeHash(hash);
        return removedMember;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void memberChanged(UInt index) {
        auto& members = getMembers<InnerViewType>();
        HashType oldHash = indexHashMap[index];
        HashType newHash = getValueHash(
            members[index]->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
        if (oldHash == newHash) {
            return;
        }
        indexHashMap[index] = newHash;
        removeHash(oldHash);
        addHash(newHash);
        debug_code(standardSanityChecksForThisType());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void membersChanged(const std::vector<UInt>& indices) {
        for (auto index : indices) {
            memberChanged<InnerViewType>(index);
        }
    }

    void silentClear() {
        lib::visit([&](auto& membersImpl) { membersImpl.clear(); }, members);
        indexHashMap.clear();
        memberCounts.clear();
        cachedHashTotal = HashType(0);
    }

   public:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMemberAndNotify(const ExprRef<InnerViewType>& member) {
        addMember(member);
        notifyMemberAdded(getMembers<InnerViewType>().back());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMemberAndNotify(UInt index) {
        ExprRef<InnerViewType> removedValue =
            removeMember<InnerViewType>(index);
        notifyMemberRemoved(index, removedValue);
        return removedValue;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void memberChangedAndNotify(size_t index) {
        HashType oldHash = indexHashMap[index];
        memberChanged<InnerViewType>(index);
        notifyMemberChanged(index,oldHash);
    }
    virtual inline AnyExprVec& getChildrenOperands() { shouldNotBeCalledPanic; }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getMembers() {
        return lib::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getMembers() const {
        return lib::get<ExprRefVec<InnerViewType>>(members);
    }

    inline UInt numberElements() const { return indexHashMap.size(); }
    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_MSET_H_ */
