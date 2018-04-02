
#ifndef SRC_OPERATORS_OPPROD_H_
#define SRC_OPERATORS_OPPROD_H_
#include <vector>
#include "types/int.h"
#include "types/sequence.h"
struct OpProd : public IntView {
    class OperandsSequenceTrigger;
    ExprRef<SequenceView> operands;
    std::shared_ptr<OperandsSequenceTrigger> operandsSequenceTrigger;

   public:
    OpProd(ExprRef<SequenceView> operands) : operands(std::move(operands)) {}
    OpProd(const OpProd& other) = delete;
    OpProd(OpProd&& other);
    virtual ~OpProd() { this->stopTriggering(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<IntView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPPROD_H_ */
