
#ifndef SRC_UTILS_SIMPLECACHE_H_
#define SRC_UTILS_SIMPLECACHE_H_
#include "common/common.h"
template <typename T>
class SimpleCache {
    bool valid = false;
    T value;

   public:
    SimpleCache() {}
    template <typename V>
    SimpleCache(V&& v) : value(std::forward<V>(v)) {}
    inline bool isValid() { return valid; }
    template <typename Func>
    inline const T& getOrSet(Func&& func) {
        if (!valid) {
            value = func();
        }
        return value;
    }

    template <typename Func>
    inline const T& getOrSet(Func&& func) const {
        return const_cast<SimpleCache<T>&>(*this).getOrSet(
            std::forward<Func>(func));
    }

    template <typename V>
    inline void set(V&& v) {
        value = std::forward<V>(v);
        valid = true;
    }

    template <typename Func>
    inline void applyIfValid(Func&& func) {
        if (isValid()) {
            func(value);
        }
    }
    template <typename Func>
    inline void applyIfValid(Func&& func) const {
        if (isValid()) {
            func(value);
        }
    }
    inline void invalidate() { valid = false; }
};
#endif /* SRC_UTILS_SIMPLECACHE_H_ */
