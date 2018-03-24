
#ifndef SRC_OPERATORS_OPAND_H_
#define SRC_OPERATORS_OPAND_H_
#include <vector>
#include "operators/quantifierView.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
class OpAndTrigger;
struct OpAnd : public BoolView {
    class QuantifierTrigger;
    std::shared_ptr<QuantifierView<BoolView>> quantifier;
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    std::vector<std::shared_ptr<OpAndTrigger>> operandTriggers;
    std::shared_ptr<QuantifierTrigger> quantifierTrigger;

   public:
    OpAnd(std::shared_ptr<QuantifierView<BoolView>> quantifier)
        : quantifier(std::move(quantifier)) {}
    OpAnd(const OpAnd& other) = delete;
    OpAnd(OpAnd&& other);
    virtual ~OpAnd() { this->stopTriggering(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPAND_H_ */
