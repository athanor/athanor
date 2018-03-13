
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
    void updateViolationDescription(u_int64_t parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopySelfForUnroll(const ExprRef<BoolView>& op,
                                        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    virtual ~OpIntEq() { this->stopTriggering(); }
};
#endif /* SRC_OPERATORS_OPINTEQ_H_ */
