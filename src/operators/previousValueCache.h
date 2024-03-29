
#ifndef SRC_OPERATORS_PREVIOUSVALUECACHE_H_
#define SRC_OPERATORS_PREVIOUSVALUECACHE_H_
#include <cassert>
#include <iostream>
#include <unordered_map>

#include "common/common.h"

template <typename Value>
struct PreviousValueCache {
    std::vector<Value> contents;
    template <typename V>
    void set(size_t index, V&& value) {
        debug_code(assert(index < contents.size()));
        contents[index] = std::forward<V>(value);
    }

    Value& get(size_t index) {
        debug_code(assert(index < contents.size()));
        return contents[index];
    }
    inline size_t size() const { return contents.size(); }
    const Value& get(size_t index) const {
        debug_code(assert(index < contents.size()));
        return contents[index];
    }

    template <typename V>
    Value getAndSet(size_t index, V&& value) {
        Value oldValue = get(index);
        set(index, std::forward<V>(value));
        return oldValue;
    }
    template <typename V>
    void insert(size_t index, V&& value) {
        debug_code(assert(index <= contents.size()));
        contents.insert(contents.begin() + index, std::forward<V>(value));
    }
    Value erase(size_t index) {
        Value oldValue = get(index);
        contents.erase(contents.begin() + index);
        return oldValue;
    }

    Value swapErase(size_t index) {
        debug_code(assert(index < contents.size()));
        std::swap(get(index), get(contents.size() - 1));
        return erase(contents.size() - 1);
    }
    void clear() { contents.clear(); }
    inline bool operator==(const PreviousValueCache<Value>& other) const {
        return contents == other.contents;
    }
    friend inline std::ostream& operator<<(
        std::ostream& os, const PreviousValueCache<Value>& other) {
        return os << other.contents;
    }
};
#endif /* SRC_OPERATORS_PREVIOUSVALUECACHE_H_ */
