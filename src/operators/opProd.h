#ifndef SRC_OPERATORS_OPPROD_H_
#define SRC_OPERATORS_OPPROD_H_
#include <vector>
#include "operators/operatorBase.h"
#include "operators/quantifierView.h"
#include "types/int.h"
class OpProdTrigger;
struct OpProd : public IntView {
    class QuantifierTrigger;
    std::shared_ptr<QuantifierView<IntReturning>> quantifier;
    std::shared_ptr<OpProdTrigger> operandTrigger;
    std::shared_ptr<QuantifierTrigger> quantifierTrigger;

   public:
    OpProd(std::shared_ptr<QuantifierView<IntReturning>> quantifier)
        : quantifier(std::move(quantifier)) {}
    OpProd(const OpProd& other) = delete;
    OpProd(OpProd&& other);
    ~OpProd() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPPROD_H_ */
