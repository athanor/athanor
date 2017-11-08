
#ifndef SRC_OPERATORS_OPSUM_H_
#define SRC_OPERATORS_OPSUM_H_
#include <vector>
#include "operators/operatorBase.h"
#include "types/int.h"
struct OpSum : public IntView {
    std::vector<IntReturning> operands;

    OpSum(std::vector<IntReturning> operandsIn)
        : operands(std::move(operandsIn)) {}
};

#endif /* SRC_OPERATORS_OPSUM_H_ */
