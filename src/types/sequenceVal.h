#ifndef SRC_TYPES_SEQUENCEVAL_H_
#define SRC_TYPES_SEQUENCEVAL_H_
#include <vector>
#include "common/common.h"
#include "types/sequence.h"
#include "types/sizeAttr.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

struct SequenceDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    bool injective;
    template <typename DomainType>
    SequenceDomain(SizeAttr sizeAttr, DomainType&& inner,
                   bool injective = false)
        : sizeAttr(sizeAttr),
          inner(makeAnyDomainRef(std::forward<DomainType>(inner))),
          injective(injective) {
        checkMaxSize();
    }

   private:
    void checkMaxSize() {
        if (sizeAttr.sizeType == SizeAttr::SizeAttrType::NO_SIZE ||
            sizeAttr.sizeType == SizeAttr::SizeAttrType::MIN_SIZE) {
            std::cerr << "Error, Sequence domain must be initialised with "
                         "maxSize() or exactSize()\n";
            abort();
        }
    }
};

struct SequenceValue : public SequenceView, public ValBase {
    bool injective = false;
    std::unordered_set<HashType> memberHashes;
    void silentClear() {
        SequenceView::silentClear();
        memberHashes.clear();
    }
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getMembers<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }

    template <typename InnerValueType>
    bool containsMember(const ValRef<InnerValueType>& val) {
        debug_code(assert(injective));
        return memberHashes.count(getValueHash(asView(*val)));
    }
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    void reassignIndicesToEnd(UInt start) {
        auto& members =
            getMembers<typename AssociatedViewType<InnerValueType>::type>();
        for (size_t i = start; i < members.size(); i++) {
            valBase(*assumeAsValue(members[i])).id = i;
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline bool addMember(UInt index, const ValRef<InnerValueType>& member) {
        if (injective) {
            HashType newMemberHash = getValueHash(member);
            if (memberHashes.count(newMemberHash)) {
                return false;
            } else {
                memberHashes.insert(newMemberHash);
            }
        }
        SequenceView::addMember(index, member.asExpr());

        valBase(*member).container = this;
        reassignIndicesToEnd<InnerValueType>(index);
        debug_code(assertValidState());
        debug_code(assertValidVarBases());
        return true;
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryAddMember(UInt index, const ValRef<InnerValueType>& member,
                             Func&& func) {
        HashType newMemberHash;
        if (injective) {
            newMemberHash = getValueHash(member);
            if (memberHashes.count(newMemberHash)) {
                return false;
            }
        }
        SequenceView::addMember(index, member.asExpr());
        if (func()) {
            if (injective) {
                memberHashes.insert(newMemberHash);
            }
            valBase(*member).container = this;
            reassignIndicesToEnd<InnerValueType>(index);
            SequenceView::notifyMemberAdded(index, member.asExpr());
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return true;
        } else {
            typedef
                typename AssociatedViewType<InnerValueType>::type InnerViewType;
            SequenceView::removeMember<InnerViewType>(index);
            return false;
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> removeMember(UInt index) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMemberExpr =
            SequenceView::removeMember<InnerViewType>(index);

        auto removedMember = assumeAsValue(removedMemberExpr);
        valBase(*removedMember).container = NULL;
        reassignIndicesToEnd<InnerValueType>(index);
        if (injective) {
            bool deleted = memberHashes.erase(
                getValueHash(removedMemberExpr->view().get()));
            static_cast<void>(deleted);
            debug_code(assert(deleted));
        }

        debug_code(assertValidVarBases());
        return removedMember;
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline std::pair<bool, ValRef<InnerValueType>> tryRemoveMember(
        UInt index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMemberExpr =
            SequenceView::removeMember<InnerViewType>(index);
        auto removedMember = assumeAsValue(removedMemberExpr);
        if (func()) {
            if (injective) {
                bool deleted = memberHashes.erase(
                    getValueHash(removedMemberExpr->view().get()));
                static_cast<void>(deleted);
                debug_code(assert(deleted));
            }

            valBase(*removedMember).container = NULL;
            reassignIndicesToEnd<InnerValueType>(index);
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            SequenceView::notifyMemberRemoved(index, removedMember.asExpr());
            return std::make_pair(true, std::move(removedMember));
        } else {
            SequenceView::addMember<InnerViewType>(index,
                                                   removedMember.asExpr());
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline HashType notifyPossibleSubsequenceChange(
        UInt start, UInt end, std::vector<HashType>& hashOfMembersToBeChanged) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        hashOfMembersToBeChanged.clear();
        auto& members = getMembers<InnerViewType>();
        for (size_t i = start; i < end; i++) {
            hashOfMembersToBeChanged.emplace_back(
                getValueHash(members[i]->view().get()));
        }
        return SequenceView::notifyPossibleSubsequenceChange<InnerViewType>(
            start, end);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline HashType changeSubsequence(
        UInt start, UInt end, std::vector<HashType>& hashOfMembersToBeChanged,
        HashType previousSubsequenceHash) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        HashType newHashOfPossibleChange =
            SequenceView::changeSubsequence<InnerViewType>(
                start, end, previousSubsequenceHash);
        for (HashType hash : hashOfMembersToBeChanged) {
            memberHashes.erase(hash);
        }
        hashOfMembersToBeChanged.clear();
        auto& members = getMembers<InnerViewType>();
        for (size_t i = start; i < end; i++) {
            hashOfMembersToBeChanged.emplace_back(
                getValueHash(members[i]->view()));
            memberHashes.insert(hashOfMembersToBeChanged.back());
        }
        return newHashOfPossibleChange;
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool trySwapPositions(UInt index1, UInt index2, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        SequenceView::swapPositions<InnerViewType>(index1, index2);
        if (func()) {
            auto& members = getMembers<InnerViewType>();
            valBase(*assumeAsValue(members[index1])).id = index1;
            valBase(*assumeAsValue(members[index2])).id = index2;
            SequenceView::notifyPositionsSwapped(index1, index2);
            debug_code(assertValidVarBases());
            return true;
        } else {
            SequenceView::swapPositions<InnerViewType>(index1, index2);
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool trySubsequenceChange(UInt start, UInt end,
                                     const std::vector<HashType>& hashes,
                                     HashType previousSubsequenceHash,
                                     Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        HashType cachedHashTotalBackup = HashType(0);
        cachedHashTotal.applyIfValid(
            [&](auto& v) { cachedHashTotalBackup = v; });
        SequenceView::changeSubsequence<InnerViewType>(start, end,
                                                       previousSubsequenceHash);
        if (func()) {
            if (injective) {
                for (HashType hash : hashes) {
                    memberHashes.erase(hash);
                }
                auto& members = getMembers<InnerViewType>();
                for (size_t i = start; i < end; i++) {
                    memberHashes.insert(getValueHash(members[i]->view().get()));
                }
            }
            debug_code(assertValidState());
            SequenceView::notifySubsequenceChanged(start, end);
            return true;
        } else {
            cachedHashTotal.applyIfValid(
                [&](auto& v) { v = cachedHashTotalBackup; });
            return false;
            ;
        }
    }

   private:
    template <typename InnerViewType>
    void reversePositions(UInt startIndex, UInt endIndex, bool triggerChange) {
        auto& members = getMembers<InnerViewType>();

        while (startIndex < endIndex) {
            swapPositions<InnerViewType>(startIndex, endIndex);
            if (triggerChange) {
                valBase(*assumeAsValue(members[startIndex])).id = startIndex;
                valBase(*assumeAsValue(members[endIndex])).id = endIndex;
                SequenceView::notifyPositionsSwapped(startIndex, endIndex);
            }
            ++startIndex;
            --endIndex;
        }
    }

   public:
    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool trySubsequenceReverse(UInt startIndex, UInt endIndex,
                                      Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        // perform test reverse without triggering
        reversePositions<InnerViewType>(startIndex, endIndex, false);
        if (func()) {
            // undo reverse
            reversePositions<InnerViewType>(startIndex, endIndex, false);
            // perform real reverse with triggering
            reversePositions<InnerViewType>(startIndex, endIndex, true);
            debug_code(assertValidVarBases());
            debug_code(assertValidState());
            return true;
        } else {
            reversePositions<InnerViewType>(startIndex, endIndex, false);
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return false;
        }
    }

    template <typename InnerViewType>
    void applySwaps(UInt offset, const std::vector<UInt>& swaps, bool isInverse,
                    bool trigger) {
        auto& members = getMembers<InnerViewType>();
        for (size_t i = 0; i < swaps.size(); i++) {
            size_t index = (!isInverse) ? i : swaps.size() - 1 - i;
            UInt point1 = index + offset, point2 = swaps[index] + offset;
            swapPositions<InnerViewType>(point1, point2);
            if (trigger) {
                valBase(*assumeAsValue(members[point1])).id = point1;
                valBase(*assumeAsValue(members[point2])).id = point2;
                SequenceView::notifyPositionsSwapped(point1, point2);
            }
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool trySubsequenceShuffle(UInt startIndex,
                                      const std::vector<UInt>& swaps,
                                      Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        // perform test reverse without triggering
        applySwaps<InnerViewType>(startIndex, swaps, false, false);
        if (func()) {
            // undo
            applySwaps<InnerViewType>(startIndex, swaps, true, false);
            // perform real reverse with triggering
            applySwaps<InnerViewType>(startIndex, swaps, false, true);
            debug_code(assertValidVarBases());
            debug_code(assertValidState());
            return true;
        } else {
            applySwaps<InnerViewType>(startIndex, swaps, false, false);
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return false;
        }
    }

    template <typename Func>
    bool tryAssignNewValue(SequenceValue& newvalue, Func&& func) {
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
    ExprRef<SequenceView> deepCopyForUnrollImpl(
        const ExprRef<SequenceView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<SequenceView>> optimise(PathExtension) final;
    void assertValidState();
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_SEQUENCEVAL_H_ */
