#ifndef SRC_BASE_INTSIZE_H_
#define SRC_BASE_INTSIZE_H_
#include <iostream>
#include "flat_hash_map.hpp"
typedef u_int64_t UInt;
typedef int64_t Int;
template <typename T, typename U>
using HashMap = ska::flat_hash_map<T, U>;
#endif /* SRC_BASE_INTSIZE_H_ */
