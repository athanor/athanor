
#ifndef SRC_OPERATORS_PREVIOUSVALUECACHE_H_
#define SRC_OPERATORS_PREVIOUSVALUECACHE_H_
#include <cassert>
#include <unordered_map>
#include "common/common.h"
template <typename T>
struct ValueMarkPair {
    T value;
    u_int32_t markCount = 0;
};
template <typename T>
class PreviousValueCache {
    std::unordered_map<size_t, ValueMarkPair<T>> lookup;

   public:
    void store(size_t index, T value) {
        auto& valueMarkPair = lookup[index];
        debug_code(assert(valueMarkPair.markCount == 0 ||
                          value == valueMarkPair.value));
        ++valueMarkPair.markCount;
        valueMarkPair.value = value;
    }

    T getAndSet(size_t index, T newValue) {
        auto iter = lookup.find(index);
        debug_code(assert(iter != lookup.end()));
        auto oldValue = iter->second.value;
        if (iter->second.markCount == 0) {
            lookup.erase(iter);
        } else {
            --iter->second.markCount;
            iter->second.value = newValue;
        }
        return oldValue;
    }
    T get(size_t index) {
        auto iter = lookup.find(index);
        debug_code(assert(iter != lookup.end()));
        auto oldValue = iter->second.value;
        if (iter->second.markCount == 0) {
            lookup.erase(iter);
        } else {
            --iter->second.markCount;
        }
        return oldValue;
    }
    void clear() { lookup.clear(); }
};

#endif /* SRC_OPERATORS_PREVIOUSVALUECACHE_H_ */
