
#ifndef SRC_OPERATORS_OPAND_H_
#define SRC_OPERATORS_OPAND_H_
#include <vector>
#include "operators/operatorBase.h"
#include "quantifierView.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
class OpAndTrigger;
class QuantifierTrigger;
struct OpAnd : public BoolView {
    std::shared_ptr<QuantifierView<BoolReturning>> quantifier;
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    std::vector<std::shared_ptr<OpAndTrigger>> operandTriggers;
    std::shared_ptr<QuantifierTrigger> quantifierTrigger;

   public:
    OpAnd(std::shared_ptr<QuantifierView<BoolReturning>> quantifier)
        : quantifier(std::move(quantifier)) {}
    OpAnd(const OpAnd& other) = delete;
    OpAnd(OpAnd&& other);
    ~OpAnd() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPAND_H_ */
