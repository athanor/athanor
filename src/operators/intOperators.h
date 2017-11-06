#ifndef SRC_OPERATORS_INTOPERATORS_H_
#define SRC_OPERATORS_INTOPERATORS_H_
#include <vector>
#include "operators/intReturning.h"

struct OpSum : public IntView {
    std::vector<IntReturning> operands;

    OpSum(std::vector<IntReturning> operandsIn)
        : operands(std::move(operandsIn)) {}
};

#endif /* SRC_OPERATORS_INTOPERATORS_H_ */
