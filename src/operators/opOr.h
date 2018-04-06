
#ifndef SRC_OPERATORS_OPOR_H_
#define SRC_OPERATORS_OPOR_H_
#include <vector>
#include "types/bool.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"
struct OpOr : public BoolView {
    class OperandsSequenceTrigger;
    ExprRef<SequenceView> operands;
    FastIterableIntSet minViolationIndices = FastIterableIntSet(0, 0);
    std::shared_ptr<OperandsSequenceTrigger> operandsSequenceTrigger;

   public:
    OpOr(ExprRef<SequenceView> operands) : operands(std::move(operands)) {}
    OpOr(const OpOr& other) = delete;
    OpOr(OpOr&& other);
    virtual ~OpOr() { this->stopTriggeringOnChildren(); }
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
};

#endif /* SRC_OPERATORS_OPOR_H_ */
