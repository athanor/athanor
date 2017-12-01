
#ifndef SRC_UTILS_CACHEDSHAREDPTR_H_
#define SRC_UTILS_CACHEDSHAREDPTR_H_
#include <memory>
#include <vector>

template <typename T>
std::shared_ptr<T> makeShared();

template <typename T>
void saveMemory(std::shared_ptr<T>& ref);
template <typename T>
class CachedSharedPtr {
   public:
    typedef T element_type;

   private:
    std::shared_ptr<T> ref;

   public:
    CachedSharedPtr(std::shared_ptr<T> ref) : ref(std::move(ref)) {}
    ~CachedSharedPtr() {
        if (!ref) {
            return;
        }
        if (ref.use_count() == 1) {
            reset(*ref);
            saveMemory(ref);
        }
    }
    inline explicit operator bool() const noexcept {
        return ref.operator bool();
    }
    inline T& operator*() const { return ref.operator*(); }
    inline T* operator->() const noexcept { return ref.operator->(); }
};

template <typename T>
std::vector<std::shared_ptr<T>>& getStorage() {
    static std::vector<std::shared_ptr<T>> storage;
    return storage;
}
template <typename T>
void saveMemory(std::shared_ptr<T>& ref) {
    getStorage<T>().emplace_back(std::move(ref));
}

template <typename T>
CachedSharedPtr<T> make() {
    auto& storage = getStorage<T>();
    if (storage.empty()) {
        return CachedSharedPtr<T>(makeShared<T>());
    } else {
        CachedSharedPtr<T> v(std::move(storage.back()));
        storage.pop_back();
        return v;
    }
}

#endif /* SRC_UTILS_CACHEDSHAREDPTR_H_ */
