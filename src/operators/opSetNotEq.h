
#ifndef SRC_OPERATORS_OPSETNOTEQ_H_
#define SRC_OPERATORS_OPSETNOTEQ_H_
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
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(u_int64_t parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopyForUnroll(const ExprRef<BoolView>& op,
                                        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPSETNOTEQ_H_ */
