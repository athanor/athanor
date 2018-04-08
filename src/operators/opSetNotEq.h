
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
    ~OpSetNotEq() { this->stopTriggeringOnChildren(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopySelfForUnroll(
        ExprRef<BoolView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

#endif /* SRC_OPERATORS_OPSETNOTEQ_H_ */
