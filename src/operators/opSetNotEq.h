
#ifndef SRC_OPERATORS_OPSETNOTEQ_H_
#define SRC_OPERATORS_OPSETNOTEQ_H_
#include "operators/operatorBase.h"
#include "types/bool.h"
class OpSetNotEqTrigger;
struct OpSetNotEq : public BoolView {
    SetReturning left;
    SetReturning right;
    std::shared_ptr<OpSetNotEqTrigger> trigger;
    OpSetNotEq(SetReturning leftIn, SetReturning rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
    OpSetNotEq(const OpSetNotEq& other) = delete;
    OpSetNotEq(OpSetNotEq&& other);
    ~OpSetNotEq() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPSETNOTEQ_H_ */
