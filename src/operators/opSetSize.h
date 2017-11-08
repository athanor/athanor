
#ifndef SRC_OPERATORS_OPSETSIZE_H_
#define SRC_OPERATORS_OPSETSIZE_H_
#include "operators/operatorBase.h"
#include "types/int.h"
struct OpSetSize : public IntView {
    SetReturning operand;

    OpSetSize(SetReturning operandIn) : operand(operandIn) {}
};

#endif /* SRC_OPERATORS_OPSETSIZE_H_ */
