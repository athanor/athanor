#ifndef SRC_TYPES_SET_H_
#define SRC_TYPES_SET_H_
#include <unordered_set>
#include <vector>
#include "types/forwardDecls/getDomainSize.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/typesAndDomains.h"
#include "utils/hashUtils.h"
struct SetDomain {
    SizeAttr sizeAttr;
    Domain inner;
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

struct SetTrigger {
    virtual void valueRemoved(const Value& member) = 0;
    virtual void valueAdded(const Value& member) = 0;
    virtual void possibleValueChange(const Value& member) = 0;
    virtual void valueChanged(const Value& member) = 0;
};

struct SetView {
    std::unordered_set<u_int64_t> memberHashes;
    u_int64_t cachedHashTotal;
    std::vector<std::shared_ptr<SetTrigger>> triggers;

    template <typename Inner>
    inline bool containsMember(const Inner& member) {
        return memberHashes.count(mix(getValueHash(*member)));
    }
};

template <typename Inner>
struct SetValueImpl {
    std::vector<Inner> members;

    template <typename Func, typename SetValueType>
    inline void changeMemberValue(Func&& func, SetValueType& val,
                                  size_t memberIndex) {
        u_int64_t hash = mix(getValueHash(*members[memberIndex]));
        for (auto& t : val.triggers) {
            t->possibleValueChange(members[memberIndex]);
        }
        bool valueChanged = func();
        if (!valueChanged) {
            return;
        }
        val.memberHashes.erase(hash);
        val.cachedHashTotal -= hash;
        hash = mix(getValueHash(*members[memberIndex]));
        val.memberHashes.insert(hash);
        val.cachedHashTotal += hash;
        for (auto& t : val.triggers) {
            t->valueChanged(members[memberIndex]);
        }
    }
    template <typename SetValueType>
    inline Inner removeValue(SetValueType& val, size_t memberIndex) {
        Inner member = std::move(members[memberIndex]);
        members[memberIndex] = std::move(members.back());
        members.pop_back();
        u_int64_t hash = mix(getValueHash(*member));
        val.memberHashes.erase(hash);
        val.cachedHashTotal -= hash;
        for (auto& t : val.triggers) {
            t->valueRemoved(member);
        }
        return member;
    }
    template <typename SetValueType>
    void removeAllValues(SetValueType& val) {
        while (!members.empty()) {
            removeValue(val, members.size() - 1);
        }
    }
    template <typename SetValueType>
    inline bool addValue(SetValueType& val, const Inner& member) {
        if (val.containsMember(member)) {
            return false;
        }
        members.push_back(member);
        setId(*member, getId(val));
        u_int64_t hash = mix(getValueHash(*member));
        val.memberHashes.insert(hash);
        val.cachedHashTotal += hash;
        for (auto& t : val.triggers) {
            t->valueAdded(members.back());
        }
        return true;
    }
};

template <typename T>
using SetValueImplWithValRefWrapper = SetValueImpl<ValRef<T>>;
typedef Variantised<SetValueImplWithValRefWrapper> SetValueImplVariant;

struct SetValue : public SetView, ValBase {
    SetValueImplVariant setValueImpl = SetValueImpl<ValRef<IntValue>>();
};

#endif /* SRC_TYPES_SET_H_ */
