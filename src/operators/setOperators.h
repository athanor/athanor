#ifndef SRC_OPERATORS_SETOPERATORS_H_
#define SRC_OPERATORS_SETOPERATORS_H_
#include <operators/setProducing.h>
#include <iostream>
#include <memory>
#include "operators/setProducing.h"
struct OpSetIntersect {
    SetProducing left;
    SetProducing right;
    std::unordered_set<u_int64_t> hashes;
};
#endif /* SRC_OPERATORS_SETOPERATORS_H_ */
