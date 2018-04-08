
#ifndef SRC_OPERATORS_OPSETSIZE_H_
#define SRC_OPERATORS_OPSETSIZE_H_
#include "types/int.h"
class OpSetSizeTrigger;
struct OpSetSize : public IntView {
    ExprRef<SetView> operand;
    std::shared_ptr<OpSetSizeTrigger> operandTrigger;
    OpSetSize(ExprRef<SetView> operandIn) : operand(operandIn) {}
    OpSetSize(const OpSetSize&) = delete;
    OpSetSize(OpSetSize&& other);
    ~OpSetSize() { this->stopTriggeringOnChildren(); }
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

#endif /* SRC_OPERATORS_OPSETSIZE_H_ */
