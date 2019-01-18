#ifndef SRC_TYPES_MSET_H_
#define SRC_TYPES_MSET_H_
#include <unordered_map>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "triggers/mSetTrigger.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

static const char* NO_MSET_UNDEFINED_MEMBERS =
    "Not yet handling multisets with undefined members.\n";
struct MSetView : public ExprInterface<MSetView>,
                  public TriggerContainer<MSetView> {
    friend MSetValue;
    AnyExprVec members;
    HashType cachedHashTotal = HashType(0);

   private:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMember(const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        HashType hash =
            getValueHash(member->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
        members.emplace_back(member);
        cachedHashTotal += mix(hash);
        debug_code(assertValidState());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));

        HashType hash = getValueHash(
            members[index]->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
        cachedHashTotal -= mix(hash);
        ExprRef<InnerViewType> removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        return removedMember;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType memberChanged(HashType oldHash, UInt index) {
        auto& members = getMembers<InnerViewType>();
        HashType newHash = getValueHash(
            members[index]->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
        if (newHash != oldHash) {
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
        }
        std::transform(
            indices.begin(), indices.end(), hashes.begin(), [&](UInt index) {
                return getValueHash(members[index]->view().checkedGet(
                    NO_MSET_UNDEFINED_MEMBERS));
            });
        for (HashType newHash : hashes) {
            cachedHashTotal += mix(newHash);
        }
        debug_code(assertValidState());
    }

    void silentClear() {
        mpark::visit(
            [&](auto& membersImpl) {
                cachedHashTotal = HashType(0);
                membersImpl.clear();
            },
            members);
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
    inline HashType notifyPossibleMemberChange(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assertValidState());
        HashType memberHash = getValueHash(
            members[index]->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
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
                    NO_MSET_UNDEFINED_MEMBERS));
            });
        debug_code(assertValidState());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType memberChangedAndNotify(size_t index, HashType oldHash) {
        HashType newHash = memberChanged<InnerViewType>(oldHash, index);
        if (oldHash == newHash) {
            return newHash;
        }
        notifyMemberChanged(index);
        return newHash;
    }

    inline void notifyEntireMSetChange() {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    inline void initFrom(MSetView& other) {
        members = other.members;
        cachedHashTotal = other.cachedHashTotal;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getMembers() {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getMembers() const {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    inline UInt numberElements() const {
        return mpark::visit([](auto& members) { return members.size(); },
                            members);
    }
    void assertValidState();
    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_MSET_H_ */
