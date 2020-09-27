
#ifndef SRC_OPERATORS_OPFUNCTIONDEFINED_H_
#define SRC_OPERATORS_OPFUNCTIONDEFINED_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/function.h"
#include "types/set.h"
struct OpFunctionDefined;
template <>
struct OperatorTrates<OpFunctionDefined> {
    struct OperandTrigger;
};
struct OpFunctionDefined
    : public SimpleUnaryOperator<SetView, FunctionView, OpFunctionDefined> {
    using SimpleUnaryOperator<SetView, FunctionView,
                              OpFunctionDefined>::SimpleUnaryOperator;

    void reevaluateImpl(FunctionView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpFunctionDefined& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPFUNCTIONDEFINED_H_ */
