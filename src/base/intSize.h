#ifndef SRC_BASE_INTSIZE_H_
#define SRC_BASE_INTSIZE_H_
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#ifdef WASM_TARGET
#include <sstream>
typedef int64_t UInt64;
typedef int32_t UInt32;
template <typename T, typename U>
using HashMap = std::unordered_map<T, U>;
template <typename T>
using HashSet = std::unordered_set<T>;
extern std::ostringstream myCerr;
void reportErrorToWebApp(size_t);
#define myAbort()           \
    reportErrorToWebApp(1); \
    abort()
#define myExit(x)           \
    reportErrorToWebApp(1); \
    exit(x)
#else
#include "flat_hash_map.hpp"
typedef u_int64_t UInt64;
typedef u_int32_t UInt32;
template <typename T, typename U>
using HashMap = ska::flat_hash_map<T, U>;
template <typename T>
using HashSet = ska::flat_hash_set<T>;
#define myCerr std::cerr
#define myAbort() abort()
#define myExit(x) exit(x)

#endif

typedef UInt64 UInt;
typedef int64_t Int;
#endif /* SRC_BASE_INTSIZE_H_ */
