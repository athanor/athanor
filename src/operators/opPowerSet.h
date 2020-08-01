
// this operator is no longer intended to be part of the AST during search. The
// parser should detect it and re-write it into a different expression.

#ifndef SRC_OPERATORS_OPPOWERSET_H_
#define SRC_OPERATORS_OPPOWERSET_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/set.h"
struct OpPowerSet;
template <>
struct OperatorTrates<OpPowerSet> {
    struct OperandTrigger;
};
struct OpPowerSet : public SimpleUnaryOperator<SetView, SetView, OpPowerSet> {
    using SimpleUnaryOperator<SetView, SetView,
                              OpPowerSet>::SimpleUnaryOperator;

    void reevaluateImpl(SetView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpPowerSet& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPPOWERSET_H_ */
