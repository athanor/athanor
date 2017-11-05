#ifndef SRC_OPERATORS_INTOPERATORS_H_
#define SRC_OPERATORS_INTOPERATORS_H_
#include <vector>
#include "operators/intProducing.h"

struct OpSum : public IntView {
    std::vector<IntProducing> operands;

    OpSum(std::vector<IntProducing> operandsIn)
        : operands(std::move(operandsIn)) {}
};

#endif /* SRC_OPERATORS_INTOPERATORS_H_ */
