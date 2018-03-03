#ifndef SRC_TYPES_MSET_H_
#define SRC_TYPES_MSET_H_
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "common/common.h"
#include "types/base.h"
#include "types/sizeAttr.h"
#include "types/typeOperations.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct MSetValue;
struct MSetView;

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
    virtual void valueAdded(const AnyValRef& member) = 0;
    virtual void possibleMemberValueChange(u_int64_t index,
                                           const AnyValRef& member) = 0;
    virtual void memberValueChanged(u_int64_t index,
                                    const AnyValRef& member) = 0;

    virtual void mSetValueChanged(const MSetView& newValue) = 0;
};
struct MSetView {
    AnyVec members;
    u_int64_t hashOfPossibleChange;
    std::vector<std::shared_ptr<MSetTrigger>> triggers;

    inline void initFrom(MSetView& other) { members = other.members; }
    template <typename InnerValueType>
    inline void notifyPossibleMemberChange(u_int64_t index) {
        auto& members = getMembers<InnerValueType>();
        debug_code(assert(index < members.size()));
        hashOfPossibleChange = getValueHash(*members[index]);
        AnyValRef triggerMember = members[index];
        debug_log("MSet possible value change, member="
                  << members[index] << " hash = " << hashOfPossibleChange);
        visitTriggers(
            [&](auto& t) {
                t->possibleMemberValueChange(index, triggerMember);
            },
            triggers);
    }
    template <typename InnerValueType>
    inline u_int64_t memberChanged(u_int64_t index) {
        auto& members = getMembers<InnerValueType>();
        debug_code(assert(index < members.size()));
        u_int64_t newHash = getValueHash(*members[index]);
        return newHash;
    }

    inline void notifyMemberChanged(size_t index,
                                    const AnyValRef& changedMember) {
        visitTriggers(
            [&](auto& t) { t->memberValueChanged(index, changedMember); },
            triggers);
    }
    template <typename InnerValueType>
    inline ValRefVec<InnerValueType>& getMembers() {
        return mpark::get<ValRefVec<InnerValueType>>(members);
    }

    inline u_int64_t numberElements() const {
        return mpark::visit([](auto& members) { return members.size(); },
                            members);
    }
};

struct MSetValue : public MSetView, public ValBase {
    template <typename InnerValueType, typename Func>
    inline bool tryMemberChange(size_t index, Func&& func) {
        u_int64_t oldHash = hashOfPossibleChange;
        hashOfPossibleChange = memberChanged<InnerValueType>(index);
        if (func()) {
            MSetView::notifyMemberChanged(index,
                                          getMembers<InnerValueType>()[index]);
            return true;
        } else {
            if (oldHash != hashOfPossibleChange) {
                hashOfPossibleChange = oldHash;
            }
            return false;
        }
    }

    template <typename InnerValueType>
    inline ValRef<InnerValueType> removeMember(u_int64_t index) {
        auto& members = getMembers<InnerValueType>();
        debug_code(assert(index < members.size()));
        debug_log("MSet removing value " << *members[index]);
        ValRef<InnerValueType> removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        valBase(*removedMember).container = NULL;
        return removedMember;
    }

    template <typename InnerValueType>
    inline void addMember(const ValRef<InnerValueType>& member) {
        auto& members = getMembers<InnerValueType>();
        debug_log("MSet adding value " << *member);
        members.emplace_back(member);
        valBase(*members.back()).container = this;
        valBase(*members.back()).id = members.size() - 1;
    }
};

template <>
struct DefinedTrigger<MSetValue> : public MSetTrigger {
    ValRef<MSetValue> val;
    DefinedTrigger(const ValRef<MSetValue>& val) : val(val) {}
    inline void valueRemoved(u_int64_t indexOfRemovedValue,
                             u_int64_t hashOfRemovedValue) {
        todoImpl(indexOfRemovedValue, hashOfRemovedValue);
    }
    inline void valueAdded(const AnyValRef& member) { todoImpl(member); }
    virtual inline void possibleMemberValueChange(u_int64_t index,
                                                  const AnyValRef& member) {
        todoImpl(index, member);
    }
    virtual void memberValueChanged(u_int64_t index, const AnyValRef& member) {
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
