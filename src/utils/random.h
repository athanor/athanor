
#ifndef SRC_UTILS_RANDOM_H_
#define SRC_UTILS_RANDOM_H_
#include <random>
extern std::mt19937 globalRandomGenerator;
template <typename IntType>
inline IntType globalRandom(IntType start, IntType end) {
    std::uniform_int_distribution<IntType> distr(start, end);
    return distr(globalRandomGenerator);
}

#endif /* SRC_UTILS_RANDOM_H_ */
