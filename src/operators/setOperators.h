#ifndef SRC_OPERATORS_SETOPERATORS_H_
#define SRC_OPERATORS_SETOPERATORS_H_
#include "operators/setProducing.h"
#include "utils/hashUtils.h"

#include <iostream>
#include <memory>
#include <vector>
#include "operators/intProducing.h"
#include "operators/setProducing.h"
#include "types/forwardDecls/hash.h"
struct OpSetIntersect {
    SetProducing left;
    SetProducing right;
    std::unordered_set<u_int64_t> memberHashes;
    u_int64_t cachedHashTotal = 0;
    std::vector<std::shared_ptr<SetTrigger>> triggers;

    OpSetIntersect(SetProducing leftIn, SetProducing rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
};

struct OpSetSize {
    int64_t value = 0;
    SetProducing operand;
    std::vector<std::shared_ptr<IntTrigger>> triggers;

    OpSetSize(SetProducing operandIn) : operand(operandIn) {}
};
#endif /* SRC_OPERATORS_SETOPERATORS_H_ */
