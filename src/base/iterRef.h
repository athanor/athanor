/*
 * iterRef.h
 *
 *  Created on: 13 Mar 2018
 *      Author: SaadAttieh
 */

#ifndef SRC_BASE_ITERREF_H_
#define SRC_BASE_ITERREF_H_
#include <vector>
#include "base/exprRef.h"
#include "base/triggers.h"
#include "base/viewRef.h"
template <typename T>
struct Iterator {
    u_int64_t id;
    ExprRef<T> ref;
    std::vector<std::shared_ptr<IterAssignedTrigger<T>>> unrollTriggers;

    Iterator(u_int64_t id, ExprRef<T> ref) : id(id), ref(std::move(ref)) {}
    template <typename Func>
    inline void changeValue(bool triggering, const ExprRef<T>& oldVal,
                            const ExprRef<T>& newVal, Func&& callback) {
        ref = newVal;
        callback();
        if (triggering) {
            for (auto& trigger : unrollTriggers) {
                trigger->iterHasNewValue(*oldVal, newVal);
            }
        }
    }
    inline ExprRef<T>& getValue() { return ref; }
};

template <typename T>
class IterRef {
   public:
    typedef T element_type;

   private:
    std::shared_ptr<Iterator<T>> ref;

   public:
    IterRef(u_int64_t id)
        : ref(std::make_shared<Iterator<T>>(id, ViewRef<T>(nullptr))) {}

    inline Iterator<T>& getIterator() { return *ref; }
    inline decltype(auto) refCount() { return ref.use_count(); }
    inline const Iterator<T>& getIterator() const { return *ref; }
    inline T& operator*() { return ref->ref.operator*(); }
    inline T* operator->() { return ref->ref.operator->(); }
    inline T& operator*() const { return ref->ref.operator*(); }
    inline T* operator->() const { return ref->ref.operator->(); }
};

template <typename T>
using IterRefMaker = IterRef<typename AssociatedViewType<T>::type>;
typedef Variantised<IterRefMaker> AnyIterRef;

template <typename T>
struct ViewType;
template <typename T>
struct ViewType<IterRef<T>> {
    typedef T type;
};

template <typename T>
struct ViewType<std::vector<IterRef<T>>> {
    typedef T type;
};

template <typename T>
HashType getValueHash(const IterRef<T>& iter) {
    return getValueHash(*iter);
}

template <typename T>
std::ostream& prettyPrint(std::ostream& os, const IterRef<T>& iter) {
    return prettyPrint(os, *iter);
}

inline HashType getValueHash(const AnyIterRef& iter) {
    return mpark::visit([](auto& iterImpl) { return getValueHash(iterImpl); },
                        iter);
}
template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyIterRef& iter,
                                 T = 0) {
    return mpark::visit(
        [&](auto& iterImpl) -> std::ostream& {
            return prettyPrint(os, iterImpl);
        },
        iter);
}

inline std::ostream& operator<<(std::ostream& os, const AnyIterRef& iter) {
    return prettyPrint(os, iter);
}

#endif /* SRC_BASE_ITERREF_H_ */
