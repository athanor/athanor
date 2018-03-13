#ifndef SRC_OPERATORS_OPSUM_H_
#define SRC_OPERATORS_OPSUM_H_
#include <vector>
#include "operators/operatorBase.h"
#include "operators/quantifierView.h"
#include "types/int.h"
class OpSumTrigger;
struct OpSum : public IntView {
    class QuantifierTrigger;
    std::shared_ptr<QuantifierView<ExprRef<IntView>>> quantifier;
    std::shared_ptr<OpSumTrigger> operandTrigger;
    std::shared_ptr<QuantifierTrigger> quantifierTrigger;

   public:
    OpSum(std::shared_ptr<QuantifierView<ExprRef<IntView>>> quantifier)
        : quantifier(std::move(quantifier)) {}
    OpSum(const OpSum& other) = delete;
    OpSum(OpSum&& other);
    ~OpSum() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPSUM_H_ */
