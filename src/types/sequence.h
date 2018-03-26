#ifndef SRC_TYPES_SEQUENCE_H_
#define SRC_TYPES_SEQUENCE_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

struct SequenceDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    template <typename DomainType>
    SequenceDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(makeAnyDomainRef(std::forward<DomainType>(inner))) {
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
struct SequenceView;
struct SequenceTrigger : public IterAssignedTrigger<SequenceView> {
    virtual void valueRemoved(UInt indexOfRemovedValue) = 0;
    virtual void valueAdded(UInt indexOfRemovedValue,
                            const AnyExprRef& member) = 0;
    virtual void possibleSubsequenceChange(UInt indexStart, UInt indexEnd) = 0;
    virtual void subsequenceChanged(UInt indexStart, UInt indexEnd) = 0;
    virtual void beginSwaps() = 0;
    virtual void positionsSwapped(UInt index1, UInt index2) = 0;
    virtual void endSwaps() = 0;
};

struct SequenceView : public ExprInterface<SequenceView> {
    friend SequenceValue;
    AnyExprVec members;
    std::vector<std::shared_ptr<SequenceTrigger>> triggers;
    SimpleCache<HashType> cachedHashTotal;
    HashType hashOfPossibleChange;

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType calcMemberHash(UInt index,
                                   const ExprRef<InnerViewType>& expr) const {
        HashType input[2];
        HashType result[2];
        input[0] = index;
        input[1] = getValueHash(*expr);
        MurmurHash3_x64_128(((void*)input), sizeof(input), 0, result);
        return result[0] ^ result[1];
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType calcSubsequenceHash(UInt start, UInt end) const {
        HashType total = 0;
        for (size_t i = start; i < end; ++i) {
            total += calcMemberHash(i, getMembers<InnerViewType>()[i]);
        }
        return total;
    }

   private:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMember(size_t index, const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        members.insert(members.begin() + index, member);
        if (index == members.size() - 1) {
            cachedHashTotal.applyIfValid(
                [&](auto& value) { value += calcMemberHash(index, member); });
        } else {
            cachedHashTotal.invalidate();
        }
        debug_code(assertValidState());
    }

    inline void notifyMemberAdded(size_t index, const AnyExprRef& newMember) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueAdded(index, newMember); },
                      triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        cachedHashTotal.invalidate();
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));
        ExprRef<InnerViewType> removedMember = std::move(members[index]);
        members.erase(members.begin() + index);
        if (index == members.size()) {
            cachedHashTotal.applyIfValid([&](auto& value) {
                value -= calcMemberHash(index, removedMember);
            });
        } else {
            cachedHashTotal.invalidate();
        }
        return removedMember;
    }

    inline void notifyMemberRemoved(UInt index) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueRemoved(index); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType changeSubsequence(UInt startIndex, UInt endIndex) {
        HashType newHash = 0;
        cachedHashTotal.applyIfValid([&](auto& value) {
            newHash = calcSubsequenceHash<InnerViewType>(startIndex, endIndex);
            value -= hashOfPossibleChange;
            value += newHash;
        });
        debug_code(assertValidState());
        return newHash;
    }

    inline void notifySubsequenceChanged(UInt startIndex, UInt endIndex) {
        debug_code(assertValidState());
        visitTriggers(
            [&](auto& t) { t->subsequenceChanged(startIndex, endIndex); },
            triggers);
    }

    inline void notifyBeginSwaps() {
        visitTriggers([&](auto& t) { t->beginSwaps(); }, triggers);
    }
    inline void notifyEndSwaps() {
        visitTriggers([&](auto& t) { t->endSwaps(); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void swapAndNotify(UInt index1, UInt index2) {
        auto& members = getMembers<InnerViewType>();
        std::swap(members[index1], members[index2]);
        cachedHashTotal.applyIfValid([&](auto& value) {
            value -= calcMemberHash(index1, members[index2]);
            value -= calcMemberHash(index2, members[index1]);
            value += calcMemberHash(index1, members[index1]);
            value += calcMemberHash(index2, members[index2]);
        });
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->positionsSwapped(index1, index2); },
                      triggers);
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
    inline void addMemberAndNotify(UInt index,
                                   const ExprRef<InnerViewType>& member) {
        addMember(index, member);
        notifyMemberAdded(index, getMembers<InnerViewType>().back());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMemberAndNotify(UInt index) {
        ExprRef<InnerViewType> removedValue =
            removeMember<InnerViewType>(index);
        notifyMemberRemoved(index, getValueHash(*removedValue));
        return removedValue;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void notifyPossibleSubsequenceChange(UInt startIndex,
                                                UInt endIndex) {
        debug_code(assertValidState());
        if (cachedHashTotal.isValid()) {
            hashOfPossibleChange =
                calcSubsequenceHash<InnerViewType>(startIndex, endIndex);
            ;
        }
        visitTriggers(
            [&](auto& t) {
                t->possibleSubsequenceChange(startIndex, endIndex);
            },
            triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void changeSubsequenceAndNotify(UInt startIndex, UInt endIndex) {
        HashType oldHash = hashOfPossibleChange;
        hashOfPossibleChange =
            changeSubsequence<InnerViewType>(startIndex, endIndex);
        if (cachedHashTotal.isValid() && oldHash == hashOfPossibleChange) {
            return;
        }
        notifySubsequenceChanged(startIndex, endIndex);
    }

    inline void initFrom(SequenceView&) {
        std::cerr << "Deprecated, Should never be called\n"
                  << __func__ << std::endl;
        abort();
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
};

struct SequenceValue : public SequenceView, public ValBase {
    using SequenceView::silentClear;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getMembers<
                typename AssociatedViewType<InnerValueType>::type>()[index]
                .asViewRef());
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    void reassignIndicesToEnd(UInt start) {
        auto& members =
            getMembers<typename AssociatedViewType<InnerValueType>::type>();
        for (size_t i = start; i < members.size(); i++) {
            valBase(assumeAsValue(*members[i])).id = i;
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void addMember(UInt index, const ValRef<InnerValueType>& member) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        SequenceView::addMember(index,
                                ExprRef<InnerViewType>(getViewPtr(member)));
        valBase(*member).container = this;
        reassignIndicesToEnd<InnerValueType>(index);
        debug_code(assertValidVarBases());
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryAddMember(UInt index, const ValRef<InnerValueType>& member,
                             Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        SequenceView::addMember(index,
                                ExprRef<InnerViewType>(getViewPtr(member)));
        if (func()) {
            valBase(*member).container = this;
            reassignIndicesToEnd<InnerValueType>(index);
            SequenceView::notifyMemberAdded(index, member);
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
        auto removedMember = assumeAsValue(
            SequenceView::removeMember<InnerViewType>(index).asViewRef());
        valBase(*removedMember).container = NULL;
        reassignIndicesToEnd<InnerValueType>(index);
        debug_code(assertValidVarBases());
        return removedMember;
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline std::pair<bool, ValRef<InnerValueType>> tryRemoveMember(
        UInt index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember = assumeAsValue(
            SequenceView::removeMember<InnerViewType>(index).asViewRef());
        if (func()) {
            valBase(*removedMember).container = NULL;
            reassignIndicesToEnd<InnerValueType>(index);
            debug_code(assertValidVarBases());
            SequenceView::notifyMemberRemoved(
                index, getValueHash(*getViewPtr(removedMember)));
            return std::make_pair(true, std::move(removedMember));
        } else {
            SequenceView::addMember<InnerViewType>(index,
                                                   getViewPtr(removedMember));
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void notifyPossibleSubsequenceChange(UInt start, UInt end) {
        return SequenceView::notifyPossibleSubsequenceChange<
            typename AssociatedViewType<InnerValueType>::type>(start, end);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void swapAndNotify(UInt index1, UInt index2) {
        return SequenceView::swapAndNotify<
            typename AssociatedViewType<InnerValueType>::type>(index1, index2);
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool trySubsequenceChange(UInt start, UInt end, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        hashOfPossibleChange =
            SequenceView::changeSubsequence<InnerViewType>(start, end);
        if (func()) {
            SequenceView::notifySubsequenceChanged(start, end);
            return true;
        } else {
            SequenceView::changeSubsequence<InnerViewType>(start, end);
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
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<SequenceView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
};

template <>
struct DefinedTrigger<SequenceValue> : public SequenceTrigger {
    ValRef<SequenceValue> val;
    DefinedTrigger(const ValRef<SequenceValue>& val) : val(val) {}
    inline void valueRemoved(UInt indexOfRemovedValue) {
        todoImpl(indexOfRemovedValue);
    }
    inline void valueAdded(UInt index, const AnyExprRef& member) {
        todoImpl(index, member);
    }
    virtual inline void possibleSubsequenceChange(UInt startIndex,
                                                  UInt endIndex) {
        todoImpl(startIndex, endIndex);
    }
    virtual void subsequenceChanged(UInt startIndex, UInt endIndex) {
        todoImpl(startIndex, endIndex);
    }

    void iterHasNewValue(const SequenceView&,
                         const ExprRef<SequenceView>&) final {
        assert(false);
        abort();
    }
};

#endif /* SRC_TYPES_SEQUENCE_H_ */
