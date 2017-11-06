#ifndef SRC_OPERATORS_BOOLOPERATORS_H_
#define SRC_OPERATORS_BOOLOPERATORS_H_
#include <vector>
#include "operators/boolReturning.h"
#include "utils/fastIterableIntSet.h"

struct OpAnd : public BoolView {
    std::vector<BoolReturning> operands;
    FastIterableIntSet violatingOperands;
    OpAnd(std::vector<BoolReturning> operandsIn)
        : operands(std::move(operandsIn)),
          violatingOperands(0, this->operands.size() - 1) {}
};

#endif /* SRC_OPERATORS_BOOLOPERATORS_H_ */
