#ifndef SRC_OPERATORS_INTOPERATORS_H_
#define SRC_OPERATORS_INTOPERATORS_H_
#include <vector>
#include "operators/intProducing.h"

struct OpSum {
    int64_t total = 0;
    std::vector<IntProducing> operands;
    std::vector<std::shared_ptr<IntTrigger>> triggers;

    OpSum(std::vector<IntProducing> operandsIn)
        : operands(std::move(operandsIn)) {}
};

#endif /* SRC_OPERATORS_INTOPERATORS_H_ */
