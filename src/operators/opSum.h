#ifndef SRC_OPERATORS_OPSUM_H_
#define SRC_OPERATORS_OPSUM_H_
#include <vector>
#include "operators/quantifierView.h"
#include "types/int.h"
class OpSumTrigger;
struct OpSum : public IntView {
    class QuantifierTrigger;
    std::shared_ptr<QuantifierView<ExprRef<IntView>>> quantifier;
    std::shared_ptr<OpSumTrigger> operandTrigger;
    std::shared_ptr<QuantifierTrigger> quantifierTrigger;

   public:
    OpSum(std::shared_ptr<QuantifierView<ExprRef<IntView>>> quantifier)
        : quantifier(std::move(quantifier)) {}
    OpSum(const OpSum& other) = delete;
    OpSum(OpSum&& other);
    ~OpSum() { this->stopTriggering(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(u_int64_t parentViolation,
                                    ViolationDescription&) final;
    ExprRef<IntView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPSUM_H_ */
