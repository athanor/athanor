
#ifndef SRC_OPERATORS_OPMOD_H_
#define SRC_OPERATORS_OPMOD_H_
#include "types/int.h"

struct OpMod : public IntView {
    struct Trigger;
    ExprRef<IntView> left;
    ExprRef<IntView> right;
    std::shared_ptr<Trigger> operandTrigger = nullptr;

   public:
    OpMod(ExprRef<IntView> left, ExprRef<IntView> right)
        : left(std::move(left)), right(std::move(right)) {}

    OpMod(const OpMod& other) = delete;
    OpMod(OpMod&& other);
    ~OpMod() { this->stopTriggeringOnChildren(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<IntView> deepCopySelfForUnroll(
        const ExprRef<IntView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

#endif /* SRC_OPERATORS_OPMOD_H_ */
