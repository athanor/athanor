#ifndef SRC_OPERATORS_OPPROD_H_
#define SRC_OPERATORS_OPPROD_H_
#include <vector>
#include "operators/quantifierView.h"
#include "types/int.h"
class OpProdTrigger;
struct OpProd : public IntView {
    class QuantifierTrigger;
    std::shared_ptr<QuantifierView<IntView>> quantifier;
    std::shared_ptr<OpProdTrigger> operandTrigger;
    std::shared_ptr<QuantifierTrigger> quantifierTrigger;

   public:
    OpProd(std::shared_ptr<QuantifierView<IntView>> quantifier)
        : quantifier(std::move(quantifier)) {}
    OpProd(const OpProd& other) = delete;
    OpProd(OpProd&& other);
    ~OpProd() { this->stopTriggering(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(u_int64_t parentViolation,
                                    ViolationDescription&) final;
    ExprRef<IntView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPPROD_H_ */
