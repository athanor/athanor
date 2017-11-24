
#ifndef SRC_OPERATORS_QUANTIFIERBASE_H_
#define SRC_OPERATORS_QUANTIFIERBASE_H_
#include <cassert>
#include <unordered_map>
#include <vector>
#include "types/forwardDecls/hash.h"
#include "types/base.h"

#include "types/forwardDecls/hash.h"

template <typename UnrollingValue>
struct IterAssignedTrigger : public virtual TriggerBase {
    virtual void iterHasNewValue(const UnrollingValue& oldValue,
                                 const ValRef<UnrollingValue>& newValue);
};

template <typename T>
struct Iterator {
    int id;
    ValRef<T> ref;
    std::vector<std::shared_ptr<IterAssignedTrigger<T>>> unrollTriggers;

    Iterator(int id, ValRef<T> ref) : id(id), ref(std::move(ref)) {}
    inline void attachValue(const ValRef<T>& val) {
        auto oldRef = std::move(ref);
        ref = val;
        if (!oldRef) {
            return;
        }
        for (auto& trigger : unrollTriggers) {
            trigger->iterHasNewValue(*oldRef, ref);
        }
    }
};

template <typename T>
class IterRef {
   public:
    typedef T element_type;

   private:
    std::shared_ptr<Iterator<T>> ref;

   public:
    IterRef(int id)
        : ref(std::make_shared<Iterator<T>>(id, ValRef<T>(nullptr))) {}
    inline Iterator<T>& getIterator() { return *ref; }
    inline decltype(auto) refCount() { return ref.use_count(); }
    inline const Iterator<T>& getIterator() const { return *ref; }
    inline decltype(auto) operator*() { return ref->ref.operator*(); }
    inline decltype(auto) operator-> () { return ref->ref.operator->(); }
    inline decltype(auto) operator*() const { return ref->ref.operator*(); }
    inline decltype(auto) operator-> () const { return ref->ref.operator->(); }
};

typedef Variantised<IterRef> AnyIterRef;

template <typename T>
u_int64_t getValueHash(const Iterator<T>& iter) {
    return getValueHash(*(iter.ref));
}

template <typename T>
u_int64_t getValueHash(const IterRef<T>& iter) {
    return getValueHash(*iter);
}

inline u_int64_t getValueHash(const AnyIterRef& iter) {
    return mpark::visit([](auto& iterImpl) { return getValueHash(iterImpl); },
                        iter);
}

#endif /* SRC_OPERATORS_QUANTIFIERBASE_H_ */
