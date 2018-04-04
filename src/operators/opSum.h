
#ifndef SRC_OPERATORS_OPSUM_H_
#define SRC_OPERATORS_OPSUM_H_
#include <vector>
#include "types/int.h"
#include "types/sequence.h"
struct OpSum : public IntView {
    class OperandsSequenceTrigger;
    ExprRef<SequenceView> operands;
    std::shared_ptr<OperandsSequenceTrigger> operandsSequenceTrigger;

   public:
    OpSum(ExprRef<SequenceView> operands) : operands(std::move(operands)) {}
    OpSum(const OpSum& other) = delete;
    OpSum(OpSum&& other);
    virtual ~OpSum() { this->stopTriggering(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<IntView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

#endif /* SRC_OPERATORS_OPSUM_H_ */
