#ifndef SRC_OPERATORS_BOOLOPERATORS_H_
#define SRC_OPERATORS_BOOLOPERATORS_H_
#include <vector>
#include "operators/boolProducing.h"

struct OpAnd {
    u_int64_t violation = 0;
    std::vector<BoolProducing> operands;
    std::vector<std::shared_ptr<BoolTrigger>> triggers;

    OpAnd(std::vector<BoolProducing> operandsIn)
        : operands(std::move(operandsIn)) {}
};

#endif /* SRC_OPERATORS_BOOLOPERATORS_H_ */
