
#ifndef SRC_OPERATORS_OPINTEQ_H_
#define SRC_OPERATORS_OPINTEQ_H_
#include "types/bool.h"
#include "types/int.h"
struct OpIntEq : public BoolView {
    struct Trigger;
    ExprRef<IntView> left;
    ExprRef<IntView> right;
    std::shared_ptr<Trigger> operandTrigger = nullptr;

   public:
    OpIntEq(ExprRef<IntView> left, ExprRef<IntView> right)
        : left(std::move(left)), right(std::move(right)) {}

    OpIntEq(const OpIntEq& other) = delete;
    OpIntEq(OpIntEq&& other);
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    virtual ~OpIntEq() { this->stopTriggeringOnChildren(); }
};
#endif /* SRC_OPERATORS_OPINTEQ_H_ */
