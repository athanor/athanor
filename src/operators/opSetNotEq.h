
#ifndef SRC_OPERATORS_OPSETNOTEQ_H_
#define SRC_OPERATORS_OPSETNOTEQ_H_
#include "operators/operatorBase.h"
#include "types/bool.h"
struct OpSetNotEq : public BoolView {
    SetReturning left;
    SetReturning right;

    OpSetNotEq(SetReturning leftIn, SetReturning rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
};

#endif /* SRC_OPERATORS_OPSETNOTEQ_H_ */
