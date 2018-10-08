
#ifndef SRC_BASE_STANDARDSHAREDPTR_H_
#define SRC_BASE_STANDARDSHAREDPTR_H_
#include <cassert>
#include <memory>
#include "common/common.h"
template <typename T>
class StandardSharedPtr {
   public:
    typedef T element_type;

   private:
    std::shared_ptr<T> ref;

   public:
    template <typename Ptr>
    StandardSharedPtr(Ptr&& ref) : ref(std::forward<Ptr>(ref)) {}
    template <typename Ptr>
    StandardSharedPtr(const Ptr& ref) : ref(ref) {}
    inline explicit operator bool() const noexcept {
        return ref.operator bool();
    }
    inline T& operator*() const {
        debug_code(assert(ref));
        return ref.operator*();
    }
    inline T* operator->() const noexcept {
        debug_code(assert(ref));
        return ref.operator->();
    }
    inline auto& getPtr() { return ref; }
    inline const auto& getPtr() const { return ref; }
};

#endif /* SRC_BASE_STANDARDSHAREDPTR_H_ */
