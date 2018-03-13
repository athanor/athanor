
#ifndef SRC_OPERATORS_OPSETNOTEQ_H_
#define SRC_OPERATORS_OPSETNOTEQ_H_
#include "operators/operatorBase.h"
#include "types/bool.h"
class OpSetNotEqTrigger;
struct OpSetNotEq : public BoolView {
    ExprRef<SetView> left;
    ExprRef<SetView> right;
    std::shared_ptr<OpSetNotEqTrigger> trigger;
    OpSetNotEq(ExprRef<SetView> leftIn, ExprRef<SetView> rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
    OpSetNotEq(const OpSetNotEq& other) = delete;
    OpSetNotEq(OpSetNotEq&& other);
    ~OpSetNotEq() { this->stopTriggering(); }
};

#endif /* SRC_OPERATORS_OPSETNOTEQ_H_ */
