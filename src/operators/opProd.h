
#ifndef SRC_OPERATORS_OPPROD_H_
#define SRC_OPERATORS_OPPROD_H_
#include <vector>
#include "operators/operatorBase.h"
#include "types/int.h"
class OpProdTrigger;
class OpProdTrigger;
struct OpProd : public IntView {
    std::vector<IntReturning> operands;
    std::shared_ptr<OpProdTrigger> operandTrigger = nullptr;

    OpProd(std::vector<IntReturning> operandsIn)
        : operands(std::move(operandsIn)) {}

    OpProd(const OpProd& other) = delete;
    OpProd(OpProd&& other);
    ~OpProd() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPPROD_H_ */
