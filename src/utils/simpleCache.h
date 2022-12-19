
#ifndef SRC_UTILS_SIMPLECACHE_H_
#define SRC_UTILS_SIMPLECACHE_H_
#include <utility>

#include "base/base.h"
#include "common/common.h"
template <typename T>
class SimpleCache {
    lib::optional<T> value;

   public:
    SimpleCache() {}
    template <typename V>
    SimpleCache(V&& v) : value(std::forward<V>(v)) {}
    inline bool isValid() const { return value.has_value(); }
    template <typename Func>
    inline const T& getOrSet(Func&& func) {
        if (!isValid()) {
            value = func();
        }
        return value.value();
    }

    template <typename Func>
    inline const T& getOrSet(Func&& func) const {
        return const_cast<SimpleCache<T>&>(*this).getOrSet(
            std::forward<Func>(func));
    }

    template <typename V>
    inline void set(V&& v) {
        value = std::forward<V>(v);
    }

    template <typename Func>
    inline void applyIfValid(Func&& func) {
        if (isValid()) {
            func(value.value());
        }
    }
    template <typename Func>
    inline void applyIfValid(Func&& func) const {
        if (isValid()) {
            func(value.value());
        }
    }
    template <typename Func>
    inline auto calcIfValid(Func&& func)
        -> lib::optional<BaseType<decltype(func(std::declval<T&>()))>> {
        if (isValid()) {
            return func(value.value());
        }
        return lib::nullopt;
    }

    inline void invalidate() { value = lib::nullopt; }
};
#endif /* SRC_UTILS_SIMPLECACHE_H_ */
