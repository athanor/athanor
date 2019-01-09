#ifndef SRC_TYPES_MSETVAL_H_
#define SRC_TYPES_MSETVAL_H_
#include <unordered_map>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/mSet.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct MSetDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    template <typename DomainType>
    MSetDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(makeAnyDomainRef(std::forward<DomainType>(inner))) {
        checkMaxSize();
    }

   private:
    void checkMaxSize() {
        if (sizeAttr.sizeType == SizeAttr::SizeAttrType::NO_SIZE ||
            sizeAttr.sizeType == SizeAttr::SizeAttrType::MIN_SIZE) {
            std::cerr << "Error, MSet domain must be initialised with "
                         "maxSize() or exactSize()\n";
            abort();
        }
    }
};

struct MSetValue : public MSetView, public ValBase {
    using MSetView::silentClear;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getMembers<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void addMember(const ValRef<InnerValueType>& member) {
        MSetView::addMember(member.asExpr());
        valBase(*member).container = this;
        valBase(*member).id = numberElements() - 1;
        debug_code(assertValidVarBases());
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryAddMember(const ValRef<InnerValueType>& member,
                             Func&& func) {
        MSetView::addMember(member.asExpr());
        if (func()) {
            valBase(*member).container = this;
            valBase(*member).id = numberElements() - 1;
            MSetView::notifyMemberAdded(member.asExpr());
            debug_code(assertValidVarBases());
            return true;
        } else {
            typedef
                typename AssociatedViewType<InnerValueType>::type InnerViewType;
            MSetView::removeMember<InnerViewType>(numberElements() - 1);
            return false;
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> removeMember(UInt index) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember =
            assumeAsValue(MSetView::removeMember<InnerViewType>(index));
        valBase(*removedMember).container = NULL;
        if (index < numberElements()) {
            valBase(*member<InnerValueType>(index)).id = index;
        }
        debug_code(assertValidVarBases());
        return removedMember;
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline std::pair<bool, ValRef<InnerValueType>> tryRemoveMember(
        UInt index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember =
            assumeAsValue(MSetView::removeMember<InnerViewType>(index));
        if (func()) {
            valBase(*removedMember).container = NULL;
            if (index < numberElements()) {
                valBase(*member<InnerValueType>(index)).id = index;
            }
            debug_code(assertValidVarBases());
            MSetView::notifyMemberRemoved(index, removedMember.asExpr());
            return std::make_pair(true, std::move(removedMember));
        } else {
            MSetView::addMember<InnerViewType>(removedMember.asExpr());
            auto& members = getMembers<InnerViewType>();
            std::swap(members[index], members.back());
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline HashType notifyPossibleMemberChange(UInt index) {
        return MSetView::notifyPossibleMemberChange<
            typename AssociatedViewType<InnerValueType>::type>(index);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void notifyPossibleMembersChange(const std::vector<UInt>& indices,
                                            std::vector<HashType>& hashes) {
        MSetView::notifyPossibleMembersChange<
            typename AssociatedViewType<InnerValueType>::type>(indices, hashes);
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline std::pair<bool, HashType> tryMemberChange(size_t index,
                                                     HashType oldHash,
                                                     Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        HashType newHash = memberChanged<InnerViewType>(oldHash, index);
        if (func()) {
            MSetView::notifyMemberChanged(index);
            return std::make_pair(true, newHash);
        } else {
            if (oldHash != newHash) {
                cachedHashTotal -= mix(newHash);
                cachedHashTotal += mix(oldHash);
            }
            return std::make_pair(false, oldHash);
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMembersChange(const std::vector<UInt>& indices,
                                 std::vector<HashType>& hashes, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        membersChanged<InnerViewType>(hashes, indices);
        HashType cachedHashTotalBackup = cachedHashTotal;
        if (func()) {
            MSetView::notifyMembersChanged(indices);
            return true;
        } else {
            cachedHashTotal = cachedHashTotalBackup;
            return false;
        }
    }

    template <typename Func>
    bool tryAssignNewValue(MSetValue& newvalue, Func&& func) {
        // fake putting in the value first untill func()verifies that it is
        // happy with the change
        std::swap(*this, newvalue);
        bool allowed = func();
        std::swap(*this, newvalue);
        if (allowed) {
            deepCopy(newvalue, *this);
        }
        return allowed;
    }
    void assertValidVarBases();

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void setInnerType() {
        if (mpark::get_if<ExprRefVec<InnerViewType>>(&(members)) == NULL) {
            members.emplace<ExprRefVec<InnerViewType>>();
        }
    }

    void printVarBases();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<MSetView> deepCopyForUnrollImpl(
        const ExprRef<MSetView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<MSetView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};
#endif /* SRC_TYPES_MSETVAL_H_ */
