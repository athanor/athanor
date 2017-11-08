
#ifndef SRC_OPERATORS_OPSETINTERSECT_H_
#define SRC_OPERATORS_OPSETINTERSECT_H_
#include "operators/operatorBase.h"
#include "types/set.h"
struct OpSetIntersect : public SetView {
    SetReturning left;
    SetReturning right;
    u_int64_t cachedHashTotal = 0;

    OpSetIntersect(SetReturning leftIn, SetReturning rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
};

#endif /* SRC_OPERATORS_OPSETINTERSECT_H_ */
