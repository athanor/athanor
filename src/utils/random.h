
#ifndef SRC_UTILS_RANDOM_H_
#define SRC_UTILS_RANDOM_H_
#include <cassert>
#include <random>
#include "common/common.h"
extern bool useSeedForRandom;
extern u_int64_t seedForRandom;
template <typename IntType>
inline IntType globalRandom(IntType start, IntType end) {
    static std::mt19937 globalRandomGenerator(
        (useSeedForRandom) ? seedForRandom : std::random_device()());
    debug_code(assert(end >= start));
    std::uniform_int_distribution<IntType> distr(start, end);
    return distr(globalRandomGenerator);
}

#endif /* SRC_UTILS_RANDOM_H_ */
