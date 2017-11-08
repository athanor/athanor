
#ifndef SRC_OPERATORS_OPAND_H_
#define SRC_OPERATORS_OPAND_H_
#include <vector>
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
class OpAndTrigger;
struct OpAnd : public BoolView {
    std::vector<BoolReturning> operands;
    FastIterableIntSet violatingOperands;
    std::vector<std::shared_ptr<OpAndTrigger>> operandTriggers;
    OpAnd(std::vector<BoolReturning> operandsIn)
        : operands(std::move(operandsIn)),
          violatingOperands(0, this->operands.size() - 1) {}
    OpAnd(OpAnd&& other);
};

#endif /* SRC_OPERATORS_OPAND_H_ */
