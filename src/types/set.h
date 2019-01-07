#ifndef SRC_TYPES_SET_H_
#define SRC_TYPES_SET_H_
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
    std::unordered_set<HashType> memberHashes;
    AnyExprVec members;
    HashType cachedHashTotal = 0;

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool addMember(const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        if (containsMember(member)) {
            return false;
        }

        HashType hash =
            getValueHash(member->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
        debug_code(assert(!memberHashes.count(hash)));
        members.emplace_back(member);
        memberHashes.insert(hash);
        cachedHashTotal += mix(hash);
        debug_code(assertValidState());
        return true;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));

        HashType hash = getValueHash(
            members[index]->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
        debug_code(assert(memberHashes.count(hash)));
        memberHashes.erase(hash);
        cachedHashTotal -= mix(hash);
        auto removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        return removedMember;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType memberChanged(HashType oldHash, UInt index) {
        auto& members = getMembers<InnerViewType>();
        HashType newHash = getValueHash(
            members[index]->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
        if (newHash != oldHash) {
            debug_code(assert(!memberHashes.count(newHash)));
            memberHashes.erase(oldHash);
            memberHashes.insert(newHash);
            cachedHashTotal -= mix(oldHash);
            cachedHashTotal += mix(newHash);
        }
        debug_code(assertValidState());
        return newHash;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void membersChanged(std::vector<HashType>& hashes,
                               const std::vector<UInt>& indices) {
        auto& members = getMembers<InnerViewType>();
        for (HashType oldHash : hashes) {
            cachedHashTotal -= mix(oldHash);
            bool erased = memberHashes.erase(oldHash);
            ignoreUnused(erased);
            debug_code(assert(erased));
        }
        std::transform(
            indices.begin(), indices.end(), hashes.begin(), [&](UInt index) {
                return getValueHash(members[index]->view().checkedGet(
                    NO_SET_UNDEFINED_MEMBERS));
            });
        for (HashType newHash : hashes) {
            cachedHashTotal += mix(newHash);
            debug_code(assert(!memberHashes.count(newHash)));
            memberHashes.insert(newHash);
        }
        debug_code(assertValidState());
    }

    void silentClear() {
        mpark::visit(
            [&](auto& membersImpl) {
                cachedHashTotal = 0;
                memberHashes.clear();
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
        ExprRef<InnerViewType> removedValue =
            removeMember<InnerViewType>(index);
        notifyMemberRemoved(index, getValueHash(removedValue->view().checkedGet(
                                       NO_SET_UNDEFINED_MEMBERS)));
        return removedValue;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType notifyPossibleMemberChange(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assertValidState());
        HashType memberHash = getValueHash(
            members[index]->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
        return memberHash;
    }
    template <typename InnerViewType>
    inline void notifyPossibleMembersChange(const std::vector<UInt>& indices,
                                            std::vector<HashType>& hashes) {
        auto& members = getMembers<InnerViewType>();
        hashes.resize(indices.size());
        std::transform(
            indices.begin(), indices.end(), hashes.begin(), [&](UInt index) {
                return getValueHash(members[index]->view().checkedGet(
                    NO_SET_UNDEFINED_MEMBERS));
            });
        debug_code(assertValidState());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType memberChangedAndNotify(size_t index, HashType oldHash) {
        HashType newHash = memberChanged<InnerViewType>(oldHash, index);
        if (oldHash == newHash) {
            return newHash;
        }
        notifyMemberChanged(index, oldHash);
        return newHash;
    }

    inline void notifyEntireSetChange() {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getMembers() {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getMembers() const {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool containsMember(const ExprRef<InnerViewType>& member) const {
        return containsMember(
            member->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool containsMember(const InnerViewType& member) const {
        return memberHashes.count(getValueHash(member));
    }

    inline UInt numberElements() const { return memberHashes.size(); }
    void assertValidState();
    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_SET_H_ */
