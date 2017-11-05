#ifndef SRC_OPERATORS_BOOLOPERATORS_H_
#define SRC_OPERATORS_BOOLOPERATORS_H_
#include <vector>
#include "operators/boolProducing.h"
#include "utils/fastIterableIntSet.h"

struct OpAnd : public BoolView {
    std::vector<BoolProducing> operands;
    FastIterableIntSet violatingOperands;
    OpAnd(std::vector<BoolProducing> operandsIn)
        : operands(std::move(operandsIn)),
          violatingOperands(0, this->operands.size() - 1) {}
};

#endif /* SRC_OPERATORS_BOOLOPERATORS_H_ */
