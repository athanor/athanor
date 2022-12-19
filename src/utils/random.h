
#ifndef SRC_UTILS_RANDOM_H_
#define SRC_UTILS_RANDOM_H_
#include <cassert>
#include <random>

#include "common/common.h"

extern std::mt19937 globalRandomGenerator;
template <
    typename IntType,
    typename std::enable_if<std::is_integral<IntType>::value, int>::type = 0>
inline IntType globalRandom(IntType start, IntType end) {
    debug_code(assert(end >= start));
    std::uniform_int_distribution<IntType> distr(start, end);
    return distr(globalRandomGenerator);
}

template <
    typename RealType,
    typename std::enable_if<!std::is_integral<RealType>::value, int>::type = 0>
inline RealType globalRandom(RealType start, RealType end) {
    debug_code(assert(end >= start));
    std::uniform_real_distribution<RealType> distr(start, end);
    return distr(globalRandomGenerator);
}

#endif /* SRC_UTILS_RANDOM_H_ */
