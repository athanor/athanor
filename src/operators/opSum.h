
#ifndef SRC_OPERATORS_OPSUM_H_
#define SRC_OPERATORS_OPSUM_H_
#include <vector>
#include "operators/operatorBase.h"
#include "types/int.h"
class OpSumTrigger;
class OpSumIterValueChangeTrigger;
struct OpSum : public IntView {
    std::vector<IntReturning> operands;
    std::shared_ptr<OpSumTrigger> operandTrigger = nullptr;
    std::shared_ptr<OpSumIterValueChangeTrigger> operandIterValueChangeTrigger =
        nullptr;

    OpSum(std::vector<IntReturning> operandsIn)
        : operands(std::move(operandsIn)) {}

    OpSum(const OpSum& other) = delete;
    OpSum(OpSum&& other);
    ~OpSum() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPSUM_H_ */
