
#ifndef SRC_BASE_STANDARDSHAREDPTR_H_
#define SRC_BASE_STANDARDSHAREDPTR_H_
#include <memory>
template <typename T>
class StandardSharedPtr {
   public:
    typedef T element_type;

   private:
    std::shared_ptr<T> ref;

   public:
    StandardSharedPtr(std::shared_ptr<T> ref) : ref(std::move(ref)) {}
    inline explicit operator bool() const noexcept {
        return ref.operator bool();
    }
    inline T& operator*() const { return ref.operator*(); }
    inline T* operator->() const noexcept { return ref.operator->(); }
    inline auto& getPtr() { return ref; }
    inline const auto& getPtr() const { return ref; }
};

#endif /* SRC_BASE_STANDARDSHAREDPTR_H_ */
