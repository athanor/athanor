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

    inline std::shared_ptr<SetDomain> deepCopy(std::shared_ptr<SetDomain>&) {
        auto newDomain = std::make_shared<SetDomain>(*this);
        newDomain->inner = deepCopyDomain(inner);
        return newDomain;
    }
    void merge(SetDomain& other) {
        sizeAttr.merge(other.sizeAttr);
        mergeDomains(inner, other.inner);
    }

   private:
    inline void trimMaxSize() {
        UInt innerDomainSize = getDomainSize(inner);
        if (innerDomainSize < sizeAttr.maxSize) {
            sizeAttr.maxSize = innerDomainSize;
        }
        if (sizeAttr.maxSize < sizeAttr.minSize) {
            myCerr << "Error, either by deduction or from original "
                      "specification, deduced max size of set to be less "
                      "than min size.\n"
                   << *this << std::endl;
            myAbort();
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

    AnyExprVec& getChildrenOperands() final;
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
    inline lib::optional<ValRef<InnerValueType>> tryRemoveMember(UInt index,
                                                                 Func&& func) {
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
            return std::move(removedMember);
        } else {
            SetView::addMember<InnerViewType>(removedMember.asExpr());
            auto& members = getMembers<InnerViewType>();
            std::swap(members[index], members.back());
            std::swap(indexHashMap[index], indexHashMap.back());
            std::swap(hashIndexMap.at(indexHashMap[index]),
                      hashIndexMap.at(indexHashMap.back()));
            debug_code(standardSanityChecksForThisType());
            debug_code(assertValidVarBases());
            return lib::nullopt;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMemberChange(size_t index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        debug_code(assert(index < numberElements()));
        HashType oldHash = indexHashMap[index];
        HashType newHash = memberChanged<InnerViewType>(index);
        if (func()) {
            SetView::notifyMemberChanged(index, oldHash);
            return true;
        } else {
            if (oldHash != newHash) {
                hashIndexMap[oldHash] = index;
                hashIndexMap.erase(newHash);
                indexHashMap[index] = oldHash;
                cachedHashTotal -= mix(newHash);
                cachedHashTotal += mix(oldHash);
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
        for (auto index : indices) {
            oldHashes.emplace_back(indexHashMap[index]);
        }
        HashType cachedHashTotalBackup = cachedHashTotal;
        membersChanged<InnerViewType>(indices);
        if (func()) {
            SetView::notifyMembersChanged(indices, oldHashes);
            return true;
        } else {
            cachedHashTotal = cachedHashTotalBackup;
            for (auto index : indices) {
                hashIndexMap.erase(indexHashMap[index]);
            }
            for (size_t i = 0; i < indices.size(); i++) {
                auto index = indices[i];
                auto hash = oldHashes[i];
                hashIndexMap[hash] = index;
                indexHashMap[index] = hash;
            }
            debug_code(standardSanityChecksForThisType());
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
    ExprRef<SetView> deepCopyForUnrollImpl(
        const ExprRef<SetView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<SetView>> optimiseImpl(ExprRef<SetView>&,
                                                   PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_SETVAL_H_ */
