
#ifndef SRC_OPERATORS_OPOR_H_
#define SRC_OPERATORS_OPOR_H_
#include <vector>
#include "operators/quantifierView.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
class OpOrTrigger;
struct OpOr : public BoolView {
    class QuantifierTrigger;
    std::shared_ptr<QuantifierView<ExprRef<BoolView>>> quantifier;
    FastIterableIntSet minViolationIndices = FastIterableIntSet(0, 0);
    std::vector<std::shared_ptr<OpOrTrigger>> operandTriggers;
    std::shared_ptr<QuantifierTrigger> quantifierTrigger;

   public:
    OpOr(std::shared_ptr<QuantifierView<ExprRef<BoolView>>> quantifier)
        : quantifier(std::move(quantifier)) {}
    OpOr(const OpOr& other) = delete;
    OpOr(OpOr&& other);
    ~OpOr() { this->stopTriggering(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(u_int64_t parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopyForUnroll(const ExprRef<BoolView>& op,
                                        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPOR_H_ */
