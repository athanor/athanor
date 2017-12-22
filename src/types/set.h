#ifndef SRC_TYPES_SET_H_
#define SRC_TYPES_SET_H_
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
    virtual void valueRemoved(const AnyValRef& member) = 0;
    virtual void valueAdded(const AnyValRef& member) = 0;
    virtual void possibleValueChange(const AnyValRef& member) = 0;
    virtual void valueChanged(const AnyValRef& member) = 0;
};

struct SetView {
   private:
    std::unordered_set<u_int64_t> memberHashes;
    u_int64_t cachedHashTotal;

   public:
    std::vector<std::shared_ptr<SetTrigger>> triggers;

    template <typename ValueType>
    inline bool containsMember(const ValRef<ValueType>& member) const {
        return containsMember(*member);
    }
    template <typename ValueType>
    inline bool containsMember(const ValueType& member) const {
        return memberHashes.count(getValueHash(member));
    }

    inline void addHash(u_int64_t hash) {
        bool inserted = memberHashes.insert(hash).second;
        debug_code(assert(inserted));
        static_cast<void>(inserted);
        cachedHashTotal += mix(hash);
    }
    inline void removeHash(u_int64_t hash) {
        bool removed = memberHashes.erase(hash);
        debug_code(assert(remove));
        static_cast<void>(removed);
        cachedHashTotal -= mix(hash);
    }

    inline void clear() {
        memberHashes.clear();
        cachedHashTotal = 0;
    }
    inline const std::unordered_set<u_int64_t>& getMemberHashes() const {
        return memberHashes;
    }
    inline u_int64_t getCachedHashTotal() const { return cachedHashTotal; }
    inline u_int64_t numberElements() const { return memberHashes.size(); }
};

template <typename Inner>
struct SetValueImpl {
    std::vector<Inner> members;
    u_int64_t hashOfPossibleChange;

    template <typename SetValueType>
    inline void possibleValueChange(SetValueType& val, size_t memberIndex) {
        val.assertValidState();
        hashOfPossibleChange = getValueHash(*members[memberIndex]);
        AnyValRef triggerMember = members[memberIndex];
        debug_log("Possible value change, member=" << members[memberIndex]
                                                   << " hash = "
                                                   << hashOfPossibleChange);
        visitTriggers([&](auto& t) { t->possibleValueChange(triggerMember); },
                      val.triggers);
    }

    template <typename SetValueType>
    inline void valueChangedSilent(SetValueType& val, size_t memberIndex) {
        debug_log("changing value, member=" << members[memberIndex]
                                            << " hasBeforeChange= "
                                            << hashOfPossibleChange);

        val.removeHash(hashOfPossibleChange);
        u_int64_t hash = getValueHash(*members[memberIndex]);
        val.addHash(hash);
        hashOfPossibleChange = hash;
        debug_log("New hash = " << hashOfPossibleChange);
        val.assertValidState();
    }

    template <typename SetValueType>
    inline void valueChanged(SetValueType& val, size_t memberIndex) {
        valueChangedSilent(val, memberIndex);
        val.signalValueChanged(members[memberIndex]);
    }

    template <typename Func, typename SetValueType>
    inline void changeMemberValue(Func&& func, SetValueType& val,
                                  size_t memberIndex) {
        possibleValueChange(val, memberIndex);
        if (func()) {
            valueChanged(val, memberIndex);
        }
    }

    template <typename SetValueType>
    inline Inner removeValueSilent(SetValueType& val, size_t memberIndex) {
        val.assertValidState();
        assert(memberIndex < members.size());
        Inner member = std::move(members[memberIndex]);
        valBase(*member).container = NULL;
        members[memberIndex] = std::move(members.back());
        members.pop_back();
        if (memberIndex < members.size()) {
            valBase(*members[memberIndex]).id = memberIndex;
        }
        u_int64_t hash = getValueHash(*member);
        val.removeHash(hash);
        return member;
    }

    template <typename SetValueType>
    void removeAllValues(SetValueType& val) {
        while (!members.empty()) {
            removeValue(val, members.size() - 1);
        }
    }

    template <typename SetValueType>
    inline bool addValueSilent(SetValueType& val, const Inner& member) {
        debug_log("Adding value " << *member);
        val.assertValidState();
        if (val.containsMember(member)) {
            return false;
        }
        members.push_back(member);
        ValBase& newMemberBase = valBase(*member);
        newMemberBase.id = members.size() - 1;
        newMemberBase.container = &val;
        u_int64_t hash = getValueHash(*member);
        val.addHash(hash);
        return true;
    }

    template <typename SetValueType>
    inline Inner removeValue(SetValueType& val, size_t memberIndex) {
        debug_log("Removingvalue " << *members[memberIndex]);
        Inner removedValue = removeValueSilent(val, memberIndex);
        val.signalValueRemoved(removedValue);
        return removedValue;
    }

    template <typename SetValueType>
    inline bool addValue(SetValueType& val, const Inner& member) {
        if (addValueSilent(val, member)) {
            val.signalValueAdded(members.back());
            return true;
        } else {
            return false;
        }
    }
};

template <typename T>
using SetValueImplWithValRefWrapper = SetValueImpl<ValRef<T>>;
typedef Variantised<SetValueImplWithValRefWrapper> SetValueImplVariant;
struct SetValue : public SetView, ValBase {
    SetValueImplVariant setValueImpl = SetValueImpl<ValRef<IntValue>>();

    inline void signalValueAdded(const AnyValRef& newMember) {
        assertValidState();
        visitTriggers([&](auto& t) { t->valueAdded(newMember); }, triggers);
    }

    inline void signalValueRemoved(const AnyValRef& removedMember) {
        assertValidState();
        visitTriggers([&](auto& t) { t->valueRemoved(removedMember); },
                      triggers);
    }

    void signalValueChanged(const AnyValRef& changedMember) {
        assertValidState();
        visitTriggers([&](auto& t) { t->valueChanged(changedMember); },
                      triggers);
    }
    void assertValidState();
};

#endif /* SRC_TYPES_SET_H_ */
