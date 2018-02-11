
#ifndef SRC_OPERATORS_ITERATOR_H_
#define SRC_OPERATORS_ITERATOR_H_
#include <cassert>
#include <unordered_map>
#include <vector>
#include "types/base.h"
#include "types/typeOperations.h"

template <typename T>
struct Iterator {
    int id;
    ValRef<T> ref;
    std::vector<std::shared_ptr<IterAssignedTrigger<T>>> unrollTriggers;

    Iterator(int id, ValRef<T> ref) : id(id), ref(std::move(ref)) {}
    template <typename Func>
    inline void changeValue(bool triggering, const ValRef<T>& oldVal,
                            const ValRef<T>& newVal, Func&& callback) {
        ref = newVal;
        callback();
        if (triggering) {
            for (auto& trigger : unrollTriggers) {
                trigger->iterHasNewValue(*oldVal, newVal);
            }
        }
    }
    inline ValRef<T>& getValue() { return ref; }
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
u_int64_t getValueHash(const IterRef<T>& iter) {
    return getValueHash(*iter);
}

template <typename T>
std::ostream& prettyPrint(std::ostream& os, const IterRef<T>& iter) {
    return prettyPrint(os, *iter);
}

inline u_int64_t getValueHash(const AnyIterRef& iter) {
    return mpark::visit([](auto& iterImpl) { return getValueHash(iterImpl); },
                        iter);
}
template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyIterRef& iter,
                                 T = 0) {
    return mpark::visit(
        [&](auto& iterImpl) -> std::ostream& {
            return prettyPrint(os, *iterImpl);
        },
        iter);
}

inline std::ostream& operator<<(std::ostream& os, const AnyIterRef& iter) {
    return prettyPrint(os, iter);
}

#endif /* SRC_OPERATORS_ITERATOR_H_ */