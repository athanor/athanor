#ifndef SRC_TYPES_SEQUENCE_H_
#define SRC_TYPES_SEQUENCE_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/oneOrMany.h"
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
SimpleCache
   private:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMember(size_t index, const ExprRef<InnerViewType>& member) {
        cacheValid = false;
        auto& members = getMembers<InnerViewType>();
        members.insert(members.begin() + index, member);
        debug_code(assertValidState());
    }

    inline void notifyMemberAdded(size_t index, const AnyExprRef& newMember) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueAdded(index, newMember); },
                      triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        cacheValid = false;
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));
        ExprRef<InnerViewType> removedMember = std::move(members[index]);
        members.erase(members.begin() + index);
        return removedMember;
    }

    inline void notifyMemberRemoved(UInt index) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueRemoved(index); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void changeMembers(UInt startIndex, UInt endIndex) {
        auto& members = getMembers<InnerViewType>();
        HashType newHash = getValueHash(*members[index]);
        if (newHash != oldHash) {
            cachedHashTotal -= mix(oldHash);
            cachedHashTotal += mix(newHash);
        }
        debug_code(assertValidState());
        return newHash;
    }

    inline void notifyMemberChanged(size_t index,
                                    const AnyExprRef& changedMember) {
        debug_code(assertValidState());
        visitTriggers(
            [&](auto& t) { t->memberValueChanged(index, changedMember); },
            triggers);
    }

    void silentClear() {
        mpark::visit(
            [&](auto& membersImpl) {
                cachedHashTotal = 0;
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
        notifyMemberRemoved(index, getValueHash(*removedValue));
        return removedValue;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void notifyPossibleMemberChange(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assertValidState());
        hashOfPossibleChange = getValueHash(*members[index]);
        AnyExprRef triggerMember = members[index];
        visitTriggers(
            [&](auto& t) {
                t->possibleMemberValueChange(index, triggerMember);
            },
            triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void memberChangedAndNotify(size_t index) {
        HashType oldHash = hashOfPossibleChange;
        hashOfPossibleChange = memberChanged<InnerViewType>(oldHash, index);
        if (oldHash == hashOfPossibleChange) {
            return;
        }
        notifyMemberChanged(index, getMembers<InnerViewType>()[index]);
    }

    inline void notifyEntireSequenceChange() {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->sequenceValueChanged(*this); },
                      triggers);
    }

    inline void initFrom(SequenceView& other) {
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
    inline void addMember(const ValRef<InnerValueType>& member) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        SequenceView::addMember(ExprRef<InnerViewType>(getViewPtr(member)));
        valBase(*member).container = this;
        valBase(*member).id = numberElements() - 1;
        debug_code(assertValidVarBases());
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryAddMember(const ValRef<InnerValueType>& member,
                             Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        SequenceView::addMember(ExprRef<InnerViewType>(getViewPtr(member)));
        if (func()) {
            valBase(*member).container = this;
            valBase(*member).id = numberElements() - 1;
            SequenceView::notifyMemberAdded(member);
            return true;
        } else {
            typedef
                typename AssociatedViewType<InnerValueType>::type InnerViewType;
            SequenceView::removeMember<InnerViewType>(numberElements() - 1);
            valBase(*member).container = NULL;
            return false;
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> removeMember(UInt index) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember = assumeAsValue(
            SequenceView::removeMember<InnerViewType>(index).asViewRef());
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
        auto removedMember = assumeAsValue(
            SequenceView::removeMember<InnerViewType>(index).asViewRef());
        if (func()) {
            valBase(*removedMember).container = NULL;
            if (index < numberElements()) {
                valBase(*member<InnerValueType>(index)).id = index;
            }
            debug_code(assertValidVarBases());
            SequenceView::notifyMemberRemoved(
                index, getValueHash(*getViewPtr(removedMember)));
            return std::make_pair(true, std::move(removedMember));
        } else {
            SequenceView::addMember<InnerViewType>(getViewPtr(removedMember));
            auto& members = getMembers<InnerValueType>();
            std::swap(members[index], members.back());
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void notifyPossibleMemberChange(UInt index) {
        return SequenceView::notifyPossibleMemberChange<
            typename AssociatedViewType<InnerValueType>::type>(index);
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMemberChange(size_t index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        HashType oldHash = hashOfPossibleChange;
        hashOfPossibleChange = memberChanged<InnerViewType>(oldHash, index);
        if (func()) {
            SequenceView::notifyMemberChanged(
                index, getMembers<InnerViewType>()[index]);
            return true;
        } else {
            if (oldHash != hashOfPossibleChange) {
                cachedHashTotal -= mix(hashOfPossibleChange);
                cachedHashTotal += mix(oldHash);
                hashOfPossibleChange = oldHash;
            }
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
    inline void valueRemoved(UInt indexOfRemovedValue,
                             HashType hashOfRemovedValue) {
        todoImpl(indexOfRemovedValue, hashOfRemovedValue);
    }
    inline void valueAdded(const AnyExprRef& member) { todoImpl(member); }
    virtual inline void possibleMemberValueChange(UInt index,
                                                  const AnyExprRef& member) {
        todoImpl(index, member);
    }
    virtual void memberValueChanged(UInt index, const AnyExprRef& member) {
        todoImpl(index, member);
        ;
    }

    void sequenceValueChanged(const SequenceView& newValue) {
        todoImpl(newValue);
    }
    void iterHasNewValue(const SequenceView&,
                         const ExprRef<SequenceView>&) final {
        assert(false);
        abort();
    }
};

#endif /* SRC_TYPES_SEQUENCE_H_ */
