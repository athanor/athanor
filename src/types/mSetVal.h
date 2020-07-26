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

    inline std::shared_ptr<MSetDomain> deepCopy(std::shared_ptr<MSetDomain>&) {
        auto newDomain = std::make_shared<MSetDomain>(*this);
        newDomain->inner = deepCopyDomain(inner);
        return newDomain;
    }

    void merge(MSetDomain& other) {
        sizeAttr.merge(other.sizeAttr);
        mergeDomains(inner, other.inner);
    }

   private:
    void checkMaxSize() {
        if (sizeAttr.sizeType == SizeAttr::SizeAttrType::NO_SIZE ||
            sizeAttr.sizeType == SizeAttr::SizeAttrType::MIN_SIZE) {
            myCerr << "Error, MSet domain must be initialised with "
                      "maxSize() or exactSize()\n";
            myAbort();
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

    AnyExprVec& getChildrenOperands() final;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void addMember(const ValRef<InnerValueType>& member) {
        MSetView::addMember(member.asExpr());
        valBase(*member).container = this;
        valBase(*member).id = numberElements() - 1;
        debug_code(debugSanityCheck());
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
            debug_code(debugSanityCheck());
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
        debug_code(debugSanityCheck());
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
            debug_code(debugSanityCheck());
            MSetView::notifyMemberRemoved(index, removedMember.asExpr());
            return std::make_pair(true, std::move(removedMember));
        } else {
            MSetView::addMember<InnerViewType>(removedMember.asExpr());
            auto& members = getMembers<InnerViewType>();
            std::swap(members[index], members.back());
            std::swap(indexHashMap[index], indexHashMap.back());
            debug_code(standardSanityChecksForThisType());
            debug_code(debugSanityCheck());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMemberChange(size_t index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        debug_code(assert(index < indexHashMap.size()));
        auto oldHash = indexHashMap[index];
        memberChanged<InnerViewType>(index);
        if (func()) {
            MSetView::notifyMemberChanged(index, oldHash);
            return true;
        } else {
            auto newHash = indexHashMap[index];
            if (oldHash != newHash) {
                indexHashMap[index] = oldHash;
                removeHash(newHash);
                addHash(oldHash);
            }
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMembersChange(const std::vector<UInt>& indices,
                                 Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        std::vector<HashType> oldHashes;
        for (auto& index : indices) {
            oldHashes.emplace_back(indexHashMap[index]);
        }
        membersChanged<InnerViewType>(indices);
        if (func()) {
            MSetView::notifyMembersChanged(indices, oldHashes);
            return true;
        } else {
            for (size_t i = 0; i < indices.size(); i++) {
                auto index = indices[i];
                auto oldHash = oldHashes[i];
                auto newHash = indexHashMap[index];
                indexHashMap[index] = oldHash;
                removeHash(newHash);
                addHash(oldHash);
            }
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

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void setInnerType() {
        if (lib::get_if<ExprRefVec<InnerViewType>>(&(members)) == NULL) {
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
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<MSetView>> optimiseImpl(ExprRef<MSetView>&,
                                                    PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};
#endif /* SRC_TYPES_MSETVAL_H_ */
