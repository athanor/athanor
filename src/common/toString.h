

#ifndef COMMON_TOSTRING_H
#define COMMON_TOSTRING_H

#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#ifndef WASM_TARGET
#include "flat_hash_map.hpp"
#endif
#include "tupleForEach.h"

// forward decls of operator<< for different containers

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& iterable);

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T>& iterable);

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::deque<T>& iterable);
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& iterable);
template <typename T>
std::ostream& operator<<(std::ostream& os,
                         const std::unordered_set<T>& iterable);
template <typename T, typename U>
std::ostream& operator<<(std::ostream& os,
                         const std::unordered_map<T, U>& iterable);
#ifndef WASM_TARGET
template <typename T, typename U>
std::ostream& operator<<(std::ostream& os,
                         const ska::flat_hash_map<T, U>& iterable);

template <typename T, typename U>
std::ostream& operator<<(std::ostream& os,
                         const ska::flat_hash_set<T, U>& iterable);
#endif

template <typename... Ts>
std::ostream& operator<<(std::ostream& os, const std::tuple<Ts...>& tuple);

/*varadic to string function */
template <typename... Args>
std::string toString(Args const&... args) {
    std::ostringstream result;
    int unpack[]{0, (result << args, 0)...};
    static_cast<void>(unpack);
    return result.str();
}

template <typename T>
T fromString(const std::string& str) {
    std::istringstream iss(str);
    T obj;
    iss >> std::ws >> obj >> std::ws;
    if (!iss.eof())
        throw std::runtime_error(toString("Failed to parse  ``", str, "''"));
    return obj;
}

template <typename T>
std::ostream& containerToArrayString(std::ostream& os, const T& container,
                                     const std::string& separator = ",") {
    bool first = true;
    os << '[';
    for (auto value : container) {
        if (first) {
            first = false;
        } else {
            os << separator;
        }
        os << value;
    }
    os << ']';
    return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& iterable) {
    return containerToArrayString(os, iterable);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T>& iterable) {
    return containerToArrayString(os, iterable);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::deque<T>& iterable) {
    return containerToArrayString(os, iterable);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& iterable) {
    return containerToArrayString(os, iterable);
}

template <typename T>
std::ostream& operator<<(std::ostream& os,
                         const std::unordered_set<T>& iterable) {
    return containerToArrayString(os, iterable);
}

template <typename T, typename U>
std::ostream& operator<<(std::ostream& os, const std::map<T, U>& iterable) {
    return containerToArrayString(os, iterable);
}

template <typename T, typename U>
std::ostream& operator<<(std::ostream& os,
                         const std::unordered_map<T, U>& iterable) {
    return containerToArrayString(os, iterable);
}
#ifndef WASM_TARGET
template <typename T, typename U>
std::ostream& operator<<(std::ostream& os,
                         const ska::flat_hash_map<T, U>& iterable) {
    return containerToArrayString(os, iterable);
}

template <typename T, typename U>
std::ostream& operator<<(std::ostream& os,
                         const ska::flat_hash_set<T, U>& iterable) {
    return containerToArrayString(os, iterable);
}
#endif

template <typename T, typename U>
std::ostream& operator<<(std::ostream& os, const std::pair<T, U>& pair) {
    return os << "(" << pair.first << "," << pair.second << ")";
}

template <typename... Ts>
std::ostream& operator<<(std::ostream& os, const std::tuple<Ts...>& tuple) {
    os << "(";
    bool first = true;
    forEach(tuple, [&](auto& member) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        os << member;
        return true;
    });
    os << ")";
    return os;
}

#endif  // common_TOSTRING_H
