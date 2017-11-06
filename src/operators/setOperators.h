#ifndef SRC_OPERATORS_SETOPERATORS_H_
#define SRC_OPERATORS_SETOPERATORS_H_

#include <iostream>
#include <memory>
#include <vector>
#include "operators/boolReturning.h"
#include "operators/intReturning.h"
#include "operators/setReturning.h"
#include "types/forwardDecls/hash.h"
#include "utils/hashUtils.h"

struct OpSetIntersect : public SetView {
    SetReturning left;
    SetReturning right;
    u_int64_t cachedHashTotal = 0;

    OpSetIntersect(SetReturning leftIn, SetReturning rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
};

struct OpSetSize : public IntView {
    SetReturning operand;

    OpSetSize(SetReturning operandIn) : operand(operandIn) {}
};

struct OpSetNotEq : public BoolView {
    SetReturning left;
    SetReturning right;

    OpSetNotEq(SetReturning leftIn, SetReturning rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
};

#endif /* SRC_OPERATORS_SETOPERATORS_H_ */
