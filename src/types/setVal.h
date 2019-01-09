#ifndef SRC_TYPES_SETVAL_H_
#define SRC_TYPES_SETVAL_H_
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/set.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct SetDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    template <typename DomainType>
    SetDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(makeAnyDomainRef(std::forward<DomainType>(inner))) {
        trimMaxSize();
    }

   private:
    inline void trimMaxSize() {
        UInt innerDomainSize = getDomainSize(inner);
        if (innerDomainSize < sizeAttr.maxSize) {
            sizeAttr.maxSize = innerDomainSize;
        }
        if (sizeAttr.maxSize < sizeAttr.minSize) {
            std::cerr << "Error, either by deduction or from original "
                         "specification, deduced max size of set to be less "
                         "than min size.\n"
                      << *this << std::endl;
            abort();
        }
    }
};

struct SetValue : public SetView, public ValBase {
    using SetView::silentClear;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getMembers<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline bool addMember(const ValRef<InnerValueType>& member) {
        if (SetView::addMember(member.asExpr())) {
            valBase(*member).container = this;
            valBase(*member).id = numberElements() - 1;
            debug_code(assertValidVarBases());
            return true;
        } else {
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryAddMember(const ValRef<InnerValueType>& member,
                             Func&& func) {
        if (SetView::addMember(member.asExpr())) {
            if (func()) {
                valBase(*member).container = this;
                valBase(*member).id = numberElements() - 1;
                SetView::notifyMemberAdded(member.asExpr());
                debug_code(assertValidVarBases());
                return true;
            } else {
                typedef typename AssociatedViewType<InnerValueType>::type
                    InnerViewType;
                SetView::removeMember<InnerViewType>(numberElements() - 1);
            }
        }
        return false;
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> removeMember(UInt index) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember =
            assumeAsValue(SetView::removeMember<InnerViewType>(index));
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
            assumeAsValue(SetView::removeMember<InnerViewType>(index));
        if (func()) {
            valBase(*removedMember).container = NULL;
            if (index < numberElements()) {
                valBase(*member<InnerValueType>(index)).id = index;
            }
            debug_code(assertValidVarBases());
            SetView::notifyMemberRemoved(index,
                                         getValueHash(asView(*removedMember)));
            return std::make_pair(true, std::move(removedMember));
        } else {
            SetView::addMember<InnerViewType>(removedMember.asExpr());
            auto& members = getMembers<InnerViewType>();
            std::swap(members[index], members.back());
            memberHashes.insert(getValueHash(
                members[index]->view().checkedGet(NO_SET_UNDEFINED_MEMBERS)));
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline HashType notifyPossibleMemberChange(UInt index) {
        return SetView::notifyPossibleMemberChange<
            typename AssociatedViewType<InnerValueType>::type>(index);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void notifyPossibleMembersChange(const std::vector<UInt>& indices,
                                            std::vector<HashType>& hashes) {
        SetView::notifyPossibleMembersChange<
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
            SetView::notifyMemberChanged(index, oldHash);
            return std::make_pair(true, newHash);
        } else {
            if (oldHash != newHash) {
                memberHashes.insert(oldHash);
                memberHashes.erase(newHash);
                cachedHashTotal -= mix(newHash);
                cachedHashTotal += mix(oldHash);
            }
            return std::make_pair(false, oldHash);
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMembersChange(const std::vector<UInt>& indices,
                                 const std::vector<HashType>& hashes,
                                 Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        std::vector<HashType> newHashes(hashes.begin(), hashes.end());
        membersChanged<InnerViewType>(newHashes, indices);
        HashType cachedHashTotalBackup = cachedHashTotal;
        if (func()) {
            SetView::notifyMembersChanged(indices, hashes);
            return true;
        } else {
            cachedHashTotal = cachedHashTotalBackup;
            for (auto hash : newHashes) {
                memberHashes.erase(hash);
            }
            for (auto hash : hashes) {
                memberHashes.insert(hash);
            }
            debug_code(assertValidState());
            return false;
        }
    }

    template <typename Func>
    bool tryAssignNewValue(SetValue& newvalue, Func&& func) {
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
    ExprRef<SetView> deepCopyForUnrollImpl(
        const ExprRef<SetView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<SetView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_SETVAL_H_ */
