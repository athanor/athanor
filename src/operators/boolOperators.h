#ifndef SRC_OPERATORS_BOOLOPERATORS_H_
#define SRC_OPERATORS_BOOLOPERATORS_H_
#include <vector>
#include "operators/boolProducing.h"

struct OpAnd : public BoolView {
    std::vector<BoolProducing> operands;

    OpAnd(std::vector<BoolProducing> operandsIn)
        : operands(std::move(operandsIn)) {}
};

#endif /* SRC_OPERATORS_BOOLOPERATORS_H_ */
