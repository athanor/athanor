#ifndef SRC_TYPES_SET_H_
#define SRC_TYPES_SET_H_
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/base.h"
#include "common/common.h"
#include "triggers/setTrigger.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

static const char* NO_SET_UNDEFINED_MEMBERS =
    "Not yet handling sets with undefined members.\n";

struct SetView : public ExprInterface<SetView>,
                 public TriggerContainer<SetView> {
    friend SetValue;
    HashMap<HashType, size_t> hashIndexMap;
    std::vector<HashType> indexHashMap;
    AnyExprVec members;
    HashType cachedHashTotal = HashType(0);

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool addMember(const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        if (containsMember(member)) {
            return false;
        }

        HashType hash =
            getValueHash(member->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
        debug_code(assert(!hashIndexMap.count(hash)));
        members.emplace_back(member);
        indexHashMap.emplace_back(hash);
        hashIndexMap[hash] = indexHashMap.size() - 1;
        cachedHashTotal += mix(hash);
        return true;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));
        HashType hash = indexHashMap[index];
        debug_code(
            assert(hashIndexMap.count(hash) && hashIndexMap.at(hash) == index));
        hashIndexMap.erase(hash);
        cachedHashTotal -= mix(hash);
        auto removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        std::swap(indexHashMap[index], indexHashMap.back());
        indexHashMap.pop_back();
        if (index < indexHashMap.size()) {
            hashIndexMap.at(indexHashMap[index]) = index;
        }
        return removedMember;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType memberChanged(UInt index) {
        auto& members = getMembers<InnerViewType>();
        HashType oldHash = indexHashMap[index];
        HashType newHash = getValueHash(
            members[index]->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
        if (newHash != oldHash) {
            debug_code(assert(!hashIndexMap.count(newHash)));
            hashIndexMap.erase(oldHash);
            hashIndexMap[newHash] = index;
            indexHashMap[index] = newHash;

            cachedHashTotal -= mix(oldHash);
            cachedHashTotal += mix(newHash);
        }
        return newHash;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void membersChanged(const std::vector<UInt>& indices) {
        auto& members = getMembers<InnerViewType>();
        for (auto index : indices) {
            debug_code(assert(index < indexHashMap.size()));
            HashType oldHash = indexHashMap[index];
            cachedHashTotal -= mix(oldHash);
            bool erased = hashIndexMap.erase(oldHash);
            ignoreUnused(erased);
            debug_code(assert(erased));
        }
        for (auto index : indices) {
            HashType newHash =

                getValueHash(members[index]->view().checkedGet(
                    NO_SET_UNDEFINED_MEMBERS));
            cachedHashTotal += mix(newHash);
            debug_code(assert(!hashIndexMap.count(newHash)));
            hashIndexMap[newHash] = index;
            indexHashMap[index] = newHash;
        }
    }

    void silentClear() {
        lib::visit(
            [&](auto& membersImpl) {
                cachedHashTotal = HashType(0);
                hashIndexMap.clear();
                indexHashMap.clear();
                membersImpl.clear();
            },
            members);
    }

   public:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool addMemberAndNotify(const ExprRef<InnerViewType>& member) {
        if (addMember(member)) {
            notifyMemberAdded(getMembers<InnerViewType>().back());
            return true;
        } else {
            return false;
        }
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMemberAndNotify(UInt index) {
        HashType hash = indexHashMap[index];
        auto removedMember = removeMember<InnerViewType>(index);
        notifyMemberRemoved(index, hash);
        return removedMember;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType memberChangedAndNotify(size_t index) {
        debug_code(assert(index < numberElements()));
        HashType oldHash = indexHashMap[index];
        HashType newHash = memberChanged<InnerViewType>(index);
        if (oldHash == newHash) {
            return newHash;
        }
        notifyMemberChanged(index, oldHash);
        return newHash;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void membersChangedAndNotify(std::vector<UInt> indices) {
        std::vector<HashType> oldHashes;
        for (auto index : indices) {
            debug_code(assert(index < numberElements()));
            oldHashes.emplace_back(indexHashMap[index]);
        }
        membersChanged<InnerViewType>(indices);
        notifyMembersChanged(indices, oldHashes);
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

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool containsMember(const ExprRef<InnerViewType>& member) const {
        return containsMember(
            member->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool containsMember(const InnerViewType& member) const {
        return hashIndexMap.count(getValueHash(member));
    }

    inline UInt numberElements() const { return indexHashMap.size(); }
    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_SET_H_ */
