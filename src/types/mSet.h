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
    SimpleCache<HashType> cachedHashTotal;

   private:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMember(const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        members.emplace_back(member);
        cachedHashTotal.applyIfValid([&](auto& value) {
            HashType hash = getValueHash(
                member->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
            value += mix(hash);
        });
        debug_code(standardSanityChecksForThisType());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));
        cachedHashTotal.applyIfValid([&](auto& value) {
            HashType hash = getValueHash(
                members[index]->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
            value -= mix(hash);
        });
        ExprRef<InnerViewType> removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        return removedMember;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline lib::optional<HashType> memberChanged(
        lib::optional<HashType> oldHashOption, UInt index) {
        auto& members = getMembers<InnerViewType>();
        lib::optional<HashType> newHashOption;
        cachedHashTotal.applyIfValid([&](auto& value) {
            debug_code(assert(oldHashOption));
            HashType oldHash = *oldHashOption;
            HashType newHash = getValueHash(
                members[index]->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
            newHashOption = newHash;
            if (newHash != oldHash) {
                value -= mix(oldHash);
                value += mix(newHash);
            }
        });
        debug_code(standardSanityChecksForThisType());
        return newHashOption;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void membersChanged(std::vector<HashType>& hashes,
                               const std::vector<UInt>& indices) {
        auto& members = getMembers<InnerViewType>();
        cachedHashTotal.applyIfValid([&](auto& value) {
            debug_code(assert(hashes.size() == indices.size()));
            for (HashType oldHash : hashes) {
                value -= mix(oldHash);
            }
            std::transform(
                indices.begin(), indices.end(), hashes.begin(),
                [&](UInt index) {
                    return getValueHash(members[index]->view().checkedGet(
                        NO_MSET_UNDEFINED_MEMBERS));
                });
            for (HashType newHash : hashes) {
                value += mix(newHash);
            }
        });
        debug_code(standardSanityChecksForThisType());
    }

    void silentClear() {
        mpark::visit(
            [&](auto& membersImpl) {
                cachedHashTotal.invalidate();
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
    inline lib::optional<HashType> notifyPossibleMemberChange(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(standardSanityChecksForThisType());
        if (cachedHashTotal.isValid()) {
            HashType memberHash = getValueHash(
                members[index]->view().checkedGet(NO_MSET_UNDEFINED_MEMBERS));
            return memberHash;
        } else {
            return lib::nullopt;
        }
    }

    template <typename InnerViewType>
    inline void notifyPossibleMembersChange(const std::vector<UInt>& indices,
                                            std::vector<HashType>& hashes) {
        if (cachedHashTotal.isValid()) {
            auto& members = getMembers<InnerViewType>();
            hashes.resize(indices.size());
            std::transform(
                indices.begin(), indices.end(), hashes.begin(),
                [&](UInt index) {
                    return getValueHash(members[index]->view().checkedGet(
                        NO_MSET_UNDEFINED_MEMBERS));
                });
        }
        debug_code(standardSanityChecksForThisType());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline lib::optional<HashType> memberChangedAndNotify(
        size_t index, lib::optional<HashType> oldHash) {
        auto newHash = memberChanged<InnerViewType>(oldHash, index);
        notifyMemberChanged(index);
        return newHash;
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
    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_MSET_H_ */
