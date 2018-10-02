
#ifndef SRC_OPERATORS_OPTOINT_H_
#define SRC_OPERATORS_OPTOINT_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
struct OpToInt;
template <>
struct OperatorTrates<OpToInt> {
    typedef SimpleUnaryTrigger<OpToInt, BoolTrigger> OperandTrigger;
};
struct OpToInt : public SimpleUnaryOperator<IntView, BoolView, OpToInt> {
    using SimpleUnaryOperator<IntView, BoolView, OpToInt>::SimpleUnaryOperator;

    void reevaluateImpl(BoolView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpToInt& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_OPTOINT_H_ */
