
#ifndef SRC_OPERATORS_OPAND_H_
#define SRC_OPERATORS_OPAND_H_
#include <vector>
#include "types/bool.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"
struct OpAnd : public BoolView {
    class OperandsSequenceTrigger;
    ExprRef<SequenceView> operands;
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    std::shared_ptr<OperandsSequenceTrigger> operandsSequenceTrigger;

   public:
    OpAnd(ExprRef<SequenceView> operands) : operands(std::move(operands)) {}
    OpAnd(const OpAnd& other) = delete;
    OpAnd(OpAnd&& other);
    virtual ~OpAnd() { this->stopTriggeringOnChildren(); }
    inline OpAnd& operator=(const OpAnd& other) {
        operands = other.operands;
        violatingOperands = other.violatingOperands;
        return *this;
    }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopySelfForUnroll(
        const ExprRef<BoolView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

#endif /* SRC_OPERATORS_OPAND_H_ */
