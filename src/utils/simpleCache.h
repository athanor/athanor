
#ifndef SRC_UTILS_SIMPLECACHE_H_
#define SRC_UTILS_SIMPLECACHE_H_

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
    inline const T& get(Func&& func) {
        if (!valid) {
            value = func();
        }
        return value;
    }

    template <typename Func>
    inline const T& get(Func&& func) const {
        return const_cast<SimpleCache<T>&>(*this).get(std::forward<Func>(func));
    }
    inline void invalidate() { valid = false; }
};
#endif /* SRC_UTILS_SIMPLECACHE_H_ */
