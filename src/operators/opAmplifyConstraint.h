
#ifndef SRC_OPERATORS_OPAMPLIFYCONSTRAINT_H_
#define SRC_OPERATORS_OPAMPLIFYCONSTRAINT_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
struct OpAmplifyConstraint;
template <>
struct OperatorTrates<OpAmplifyConstraint> {
    typedef SimpleUnaryTrigger<OpAmplifyConstraint, BoolTrigger> OperandTrigger;
};

struct OpAmplifyConstraint
    : public SimpleUnaryOperator<BoolView, BoolView, OpAmplifyConstraint> {
    using SimpleUnaryOperator<BoolView, BoolView,
                              OpAmplifyConstraint>::SimpleUnaryOperator;
    UInt64 multiplier;
    OpAmplifyConstraint(ExprRef<BoolView> operand, UInt64 multiplier)
        : SimpleUnaryOperator<BoolView, BoolView, OpAmplifyConstraint>(
              std::move(operand)),
          multiplier(multiplier) {}
    void reevaluateImpl(BoolView& view);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpAmplifyConstraint& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPAMPLIFYCONSTRAINT_H_ */
