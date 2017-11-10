
#ifndef SRC_OPERATORS_OPSETSIZE_H_
#define SRC_OPERATORS_OPSETSIZE_H_
#include "operators/operatorBase.h"
#include "types/int.h"
class OpSetSizeTrigger;
struct OpSetSize : public IntView {
    SetReturning operand;
    std::shared_ptr<OpSetSizeTrigger> operandTrigger;
    OpSetSize(SetReturning operandIn) : operand(operandIn) {}
    OpSetSize(const OpSetSize&) = delete;
    OpSetSize(OpSetSize&& other);
    ~OpSetSize() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPSETSIZE_H_ */
