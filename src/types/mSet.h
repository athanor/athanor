#ifndef SRC_TYPES_MSET_H_
#define SRC_TYPES_MSET_H_
#include <unordered_map>
#include <vector>
#include "common/common.h"
#include "types/base.h"
#include "types/sizeAttr.h"
#include "types/typeOperations.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct MSetDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    // template hack to accept only domains
    template <
        typename DomainType,
        typename std::enable_if<IsDomainType<DomainType>::value, int>::type = 0>
    MSetDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(std::make_shared<
                typename std::remove_reference<DomainType>::type>(
              std::forward<DomainType>(inner))) {
        checkMaxSize();
    }

    // template hack to accept only pointers to domains
    template <typename DomainPtrType,
              typename std::enable_if<IsDomainPtrType<DomainPtrType>::value,
                                      int>::type = 0>
    MSetDomain(SizeAttr sizeAttr, DomainPtrType&& inner)
        : sizeAttr(sizeAttr), inner(std::forward<DomainPtrType>(inner)) {
        checkMaxSize();
    }

    template <typename AnyDomainRefType,
              typename std::enable_if<
                  std::is_same<BaseType<AnyDomainRefType>, AnyDomainRef>::value,
                  int>::type = 0>
    MSetDomain(SizeAttr sizeAttr, AnyDomainRefType&& inner)
        : sizeAttr(sizeAttr), inner(std::forward<AnyDomainRefType>(inner)) {
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

struct MSetTrigger : public IterAssignedTrigger<MSetValue> {
    virtual void valueRemoved(u_int64_t indexOfRemovedValue,
                              u_int64_t hashOfRemovedValue) = 0;
    virtual void valueAdded(const AnyViewRef& member) = 0;
    virtual void possibleMemberValueChange(u_int64_t index,
                                           const AnyViewRef& member) = 0;
    virtual void memberValueChanged(u_int64_t index,
                                    const AnyViewRef& member) = 0;

    virtual void mSetValueChanged(const MSetView& newValue) = 0;
};

struct MSetView {
    friend MSetValue;
    AnyViewVec members;
    u_int64_t cachedHashTotal;
    u_int64_t hashOfPossibleChange;
    std::vector<std::shared_ptr<MSetTrigger>> triggers;

   private:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMember(const ViewRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        u_int64_t hash = getValueHash(*member);
        members.emplace_back(member);
        cachedHashTotal += mix(hash);
        debug_code(assertValidState());
    }

    inline void notifyMemberAdded(const AnyViewRef& newMember) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueAdded(newMember); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ViewRef<InnerViewType> removeMember(u_int64_t index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));

        u_int64_t hash = getValueHash(*members[index]);
        cachedHashTotal -= mix(hash);
        ViewRef<InnerViewType> removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        return removedMember;
    }

    inline void notifyMemberRemoved(u_int64_t index,
                                    u_int64_t hashOfRemovedMember) {
        debug_code(assertValidState());
        visitTriggers(
            [&](auto& t) { t->valueRemoved(index, hashOfRemovedMember); },
            triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline u_int64_t memberChanged(u_int64_t oldHash, u_int64_t index) {
        auto& members = getMembers<InnerViewType>();
        u_int64_t newHash = getValueHash(*members[index]);
        if (newHash != oldHash) {
            cachedHashTotal -= mix(oldHash);
            cachedHashTotal += mix(newHash);
        }
        debug_code(assertValidState());
        return newHash;
    }

    inline void notifyMemberChanged(size_t index,
                                    const AnyViewRef& changedMember) {
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
    inline void addMemberAndNotify(const ViewRef<InnerViewType>& member) {
        addMember(member);
        notifyMemberAdded(getMembers<InnerViewType>().back());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ViewRef<InnerViewType> removeMemberAndNotify(u_int64_t index) {
        ViewRef<InnerViewType> removedValue =
            removeMember<InnerViewType>(index);
        notifyMemberRemoved(index, getValueHash(*removedValue));
        return removedValue;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void notifyPossibleMemberChange(u_int64_t index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assertValidState());
        hashOfPossibleChange = getValueHash(*members[index]);
        AnyViewRef triggerMember = members[index];
        visitTriggers(
            [&](auto& t) {
                t->possibleMemberValueChange(index, triggerMember);
            },
            triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void memberChangedAndNotify(size_t index) {
        u_int64_t oldHash = hashOfPossibleChange;
        hashOfPossibleChange = memberChanged<InnerViewType>(oldHash, index);
        if (oldHash == hashOfPossibleChange) {
            return;
        }
        notifyMemberChanged(index, getMembers<InnerViewType>()[index]);
    }

    inline void notifyEntireMSetChange() {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->mSetValueChanged(*this); }, triggers);
    }

    inline void initFrom(MSetView& other) {
        members = other.members;
        cachedHashTotal = other.cachedHashTotal;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ViewRefVec<InnerViewType>& getMembers() {
        return mpark::get<ViewRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ViewRefVec<InnerViewType>& getMembers() const {
        return mpark::get<ViewRefVec<InnerViewType>>(members);
    }

    inline u_int64_t numberElements() const {
        return mpark::visit([](auto& members) { return members.size(); },
                            members);
    }
    void assertValidState();
};

struct MSetValue : public MSetView, public ValBase {
    using MSetView::silentClear;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void addMember(const ValRef<InnerValueType>& member) {
        MSetView::addMember(getViewPtr(member));
        valBase(*member).container = this;
        valBase(*member).id = numberElements() - 1;
        debug_code(assertValidVarBases());
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryAddMember(const ValRef<InnerValueType>& member,
                             Func&& func) {
        MSetView::addMember(getViewPtr(member));
        if (func()) {
            valBase(*member).container = this;
            valBase(*member).id = numberElements() - 1;
            MSetView::notifyMemberAdded(member);
            return true;
        } else {
            typedef
                typename AssociatedViewType<InnerValueType>::type InnerViewType;
            MSetView::removeMember<InnerViewType>(numberElements() - 1);
            valBase(*member).container = NULL;
            return false;
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> removeMember(u_int64_t index) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember = MSetView::removeMember<InnerViewType>(index);
        valBase(*removedMember).container = NULL;
        if (index < numberElements()) {
            valBase(*getMembers<InnerViewType>()[index]).id = index;
        }
        debug_code(assertValidVarBases());
        return assumeAsValue(removedMember);
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline std::pair<bool, ValRef<InnerValueType>> tryRemoveMember(
        u_int64_t index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember = MSetView::removeMember<InnerViewType>(index);
        if (func()) {
            valBase(*removedMember).container = NULL;
            if (index < numberElements()) {
                valBase(*getMembers<InnerViewType>()[index]).id = index;
            }
            debug_code(assertValidVarBases());
            MSetView::notifyMemberRemoved(index, getValueHash(*removedMember));
            return std::make_pair(true,
                                  std::move(assumeAsValue(removedMember)));
        } else {
            MSetView::addMember(removedMember);
            auto& members = getMembers<InnerValueType>();
            std::swap(members[index], members.back());
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMemberChange(size_t index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        u_int64_t oldHash = hashOfPossibleChange;
        hashOfPossibleChange = memberChanged<InnerViewType>(oldHash, index);
        if (func()) {
            MSetView::notifyMemberChanged(index,
                                          getMembers<InnerViewType>()[index]);
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
        if (mpark::get_if<ViewRefVec<InnerViewType>>(&(members)) == NULL) {
            members.emplace<ViewRefVec<InnerViewType>>();
        }
    }

    void printVarBases();
};

template <>
struct DefinedTrigger<MSetValue> : public MSetTrigger {
    ValRef<MSetValue> val;
    DefinedTrigger(const ValRef<MSetValue>& val) : val(val) {}
    inline void valueRemoved(u_int64_t indexOfRemovedValue,
                             u_int64_t hashOfRemovedValue) {
        todoImpl(indexOfRemovedValue, hashOfRemovedValue);
    }
    inline void valueAdded(const AnyViewRef& member) { todoImpl(member); }
    virtual inline void possibleMemberValueChange(u_int64_t index,
                                                  const AnyViewRef& member) {
        todoImpl(index, member);
    }
    virtual void memberValueChanged(u_int64_t index, const AnyViewRef& member) {
        todoImpl(index, member);
        ;
    }

    void mSetValueChanged(const MSetView& newValue) { todoImpl(newValue); }
    void iterHasNewValue(const MSetValue&, const ValRef<MSetValue>&) final {
        assert(false);
        abort();
    }
};

#endif /* SRC_TYPES_MSET_H_ */
