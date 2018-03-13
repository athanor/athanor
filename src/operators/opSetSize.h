
#ifndef SRC_OPERATORS_OPSETSIZE_H_
#define SRC_OPERATORS_OPSETSIZE_H_
#include "operators/operatorBase.h"
#include "types/int.h"
class OpSetSizeTrigger;
struct OpSetSize : public IntView {
    ExprRef<SetView> operand;
    std::shared_ptr<OpSetSizeTrigger> operandTrigger;
    OpSetSize(ExprRef<SetView> operandIn) : operand(operandIn) {}
    OpSetSize(const OpSetSize&) = delete;
    OpSetSize(OpSetSize&& other);
    ~OpSetSize() { this->stopTriggering(); }
};

#endif /* SRC_OPERATORS_OPSETSIZE_H_ */
