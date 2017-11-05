#ifndef SRC_OPERATORS_SETOPERATORS_H_
#define SRC_OPERATORS_SETOPERATORS_H_

#include <iostream>
#include <memory>
#include <vector>
#include "operators/boolProducing.h"
#include "operators/intProducing.h"
#include "operators/setProducing.h"
#include "types/forwardDecls/hash.h"
#include "utils/hashUtils.h"

struct OpSetIntersect : public SetView {
    SetProducing left;
    SetProducing right;
    u_int64_t cachedHashTotal = 0;

    OpSetIntersect(SetProducing leftIn, SetProducing rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
};

struct OpSetSize : public IntView {
    SetProducing operand;

    OpSetSize(SetProducing operandIn) : operand(operandIn) {}
};

struct OpSetNotEq : public BoolView {
    SetProducing left;
    SetProducing right;

    OpSetNotEq(SetProducing leftIn, SetProducing rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
};

#endif /* SRC_OPERATORS_SETOPERATORS_H_ */
