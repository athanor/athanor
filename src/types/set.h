#ifndef SRC_TYPES_SET_H_
#define SRC_TYPES_SET_H_
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "common/common.h"
#include "types/base.h"
#include "types/sizeAttr.h"
#include "types/typeOperations.h"
#include "utils/hashUtils.h"

struct SetDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    // template hack to accept only domains
    template <
        typename DomainType,
        typename std::enable_if<IsDomainType<DomainType>::value, int>::type = 0>
    SetDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(std::make_shared<
                typename std::remove_reference<DomainType>::type>(
              std::forward<DomainType>(inner))) {
        trimMaxSize();
    }

    // template hack to accept only pointers to domains
    template <typename DomainPtrType,
              typename std::enable_if<IsDomainPtrType<DomainPtrType>::value,
                                      int>::type = 0>
    SetDomain(SizeAttr sizeAttr, DomainPtrType&& inner)
        : sizeAttr(sizeAttr), inner(std::forward<DomainPtrType>(inner)) {
        trimMaxSize();
    }

   private:
    inline void trimMaxSize() {
        u_int64_t innerDomainSize = getDomainSize(inner);
        if (innerDomainSize < sizeAttr.maxSize) {
            sizeAttr.maxSize = innerDomainSize;
        }
    }
};

struct SetTrigger : public IterAssignedTrigger<SetValue> {
    typedef SetView View;
    virtual void valueRemoved(u_int64_t indexOfRemovedValue,
                              u_int64_t hashOfRemovedValue) = 0;
    virtual void valueAdded(const AnyValRef& member) = 0;
    virtual void possibleMemberValueChange(u_int64_t index,
                                           const AnyValRef& member) = 0;
    virtual void memberValueChanged(u_int64_t index,
                                    const AnyValRef& member) = 0;

    virtual void setValueChanged(const SetView& newValue) = 0;
};

struct SetView {
    std::unordered_map<u_int64_t, u_int64_t> hashIndexMap;
    AnyVec members;
    u_int64_t cachedHashTotal;
    u_int64_t hashOfPossibleChange;
    std::vector<std::shared_ptr<SetTrigger>> triggers;

    template <typename InnerValueType>
    inline ValRefVec<InnerValueType>& getMembers() {
        return mpark::get<ValRefVec<InnerValueType>>(members);
    }

   public:
    template <typename InnerValueType>
    inline const ValRefVec<InnerValueType>& getMembers() const {
        return mpark::get<ValRefVec<InnerValueType>>(members);
    }

    inline const AnyVec& memberVec() { return members; }

    template <typename ValueType>
    inline bool containsMember(const ValRef<ValueType>& member) const {
        return containsMember(*member);
    }
    template <typename ValueType>
    inline bool containsMember(const ValueType& member) const {
        return hashIndexMap.count(getValueHash(member));
    }
    template <typename InnerValueType>
    inline bool addMember(const ValRef<InnerValueType>& member) {
        auto& members = getMembers<InnerValueType>();
        debug_log("Adding value " << *member);
        if (containsMember(member)) {
            debug_log("rejected");
            return false;
        }

        u_int64_t hash = getValueHash(*member);
        debug_code(assert(!hashIndexMap.count(hash)));
        members.emplace_back(member);
        hashIndexMap.emplace(hash, members.size() - 1);
        cachedHashTotal += mix(hash);
        return true;
    }

    inline void notifyMemberAdded(const AnyValRef& newMember) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueAdded(newMember); }, triggers);
    }

    template <typename InnerValueType>
    inline bool addMemberAndNotify(const ValRef<InnerValueType>& member) {
        if (addMember(member)) {
            notifyMemberAdded(getMembers<InnerValueType>().back());
            return true;
        } else {
            return false;
        }
    }

    template <typename InnerValueType>
    inline ValRef<InnerValueType> removeMember(u_int64_t index) {
        auto& members = getMembers<InnerValueType>();
        debug_code(assert(index < members.size()));
        debug_log("Removing value " << *members[index]);

        u_int64_t hash = getValueHash(*members[index]);
        debug_code(assert(hashIndexMap.count(hash)));
        hashIndexMap.erase(hash);
        cachedHashTotal -= mix(hash);
        ValRef<InnerValueType> removedMember = std::move(members[index]);
        valBase(*removedMember).container = NULL;
        members[index] = std::move(members.back());
        members.pop_back();
        if (index < members.size()) {
            debug_code(
                assert(hashIndexMap.count(getValueHash(*members[index]))));
            hashIndexMap[getValueHash(*members[index])] = index;
        }
        return removedMember;
    }

    inline void notifyMemberRemoved(u_int64_t index,
                                    u_int64_t hashOfRemovedMember) {
        debug_code(assertValidState());
        visitTriggers(
            [&](auto& t) { t->valueRemoved(index, hashOfRemovedMember); },
            triggers);
    }

    template <typename InnerValueType>
    inline ValRef<InnerValueType> removeMemberAndNotify(u_int64_t index) {
        ValRef<InnerValueType> removedValue =
            removeMember<InnerValueType>(index);
        notifyMemberRemoved(index, getValueHash(*removedValue));
        return removedValue;
    }

    template <typename InnerValueType>
    inline void notifyPossibleMemberChange(u_int64_t index) {
        auto& members = getMembers<InnerValueType>();
        debug_code(assertValidState());
        hashOfPossibleChange = getValueHash(*members[index]);
        AnyValRef triggerMember = members[index];
        debug_log("Possible value change, member="
                  << members[index] << " hash = " << hashOfPossibleChange);
        visitTriggers(
            [&](auto& t) {
                t->possibleMemberValueChange(index, triggerMember);
            },
            triggers);
    }

    template <typename InnerValueType>
    inline u_int64_t memberChanged(u_int64_t oldHash, u_int64_t index) {
        auto& members = getMembers<InnerValueType>();
        u_int64_t newHash = getValueHash(*members[index]);
        debug_code(assert(!hashIndexMap.count(newHash)));
        hashIndexMap[newHash] = hashIndexMap[oldHash];
        hashIndexMap.erase(oldHash);
        return newHash;
    }

    inline void notifyMemberChanged(size_t index,
                                    const AnyValRef& changedMember) {
        debug_code(assertValidState());
        visitTriggers(
            [&](auto& t) { t->memberValueChanged(index, changedMember); },
            triggers);
    }

    template <typename InnerValueType>
    inline void memberChangedAndNotify(size_t index) {
        hashOfPossibleChange =
            memberChanged<InnerValueType>(hashOfPossibleChange, index);
        notifyMemberChanged(index, getMembers<InnerValueType>()[index]);
    }

    template <typename InnerValueType, typename Func>
    inline void changeMemberAndNotify(Func&& func, size_t index) {
        notifyPossibleMemberChange<InnerValueType>(index);
        if (func()) {
            memberChangedAndNotify<InnerValueType>(index);
        }
    }

    inline void notifyEntireSetChange() {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->setValueChanged(*this); }, triggers);
    }

    inline const std::unordered_map<u_int64_t, u_int64_t>& getHashIndexMap()
        const {
        return hashIndexMap;
    }

    inline u_int64_t getCachedHashTotal() const { return cachedHashTotal; }
    inline u_int64_t numberElements() const { return hashIndexMap.size(); }
    void silentClear() {
        mpark::visit(
            [&](auto& membersImpl) {
                cachedHashTotal = 0;
                hashIndexMap.clear();
                membersImpl.clear();

            },
            members);
    }

    void assertValidState();
};

struct SetValue : public SetView, ValBase {
    template <typename InnerValueType>
    inline bool addMember(const ValRef<InnerValueType>& member) {
        if (SetView::addMember(member)) {
            valBase(*member).container = this;
            return true;
        } else {
            return false;
        }
    }

    template <typename InnerValueType>
    inline bool addMemberAndNotify(const ValRef<InnerValueType>& member) {
        if (SetView::addMemberAndNotify(member)) {
            valBase(*member).container = this;
            return true;
        } else {
            return false;
        }
    }

    template <typename InnerValueType>
    inline ValRef<InnerValueType> removeMember(u_int64_t index) {
        auto removedMember = SetView::removeMember<InnerValueType>(index);
        valBase(*removedMember).container = NULL;
        return removedMember;
    }
    template <typename InnerValueType>
    inline ValRef<InnerValueType> removeMemberAndNotify(u_int64_t index) {
        auto removedMember =
            SetView::removeMemberAndNotify<InnerValueType>(index);
        valBase(*removedMember).container = NULL;
        return removedMember;
    }
};

#endif /* SRC_TYPES_SET_H_ */
