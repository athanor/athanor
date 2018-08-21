
#ifndef SRC_OPERATORS_OPSETSIZE_H_
#define SRC_OPERATORS_OPSETSIZE_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
#include "types/set.h"
struct OpSetSize;
template <>
struct OperatorTrates<OpSetSize> {
    typedef SimpleUnaryTrigger<OpSetSize, SetTrigger> OperandTrigger;
};
struct OpSetSize : public SimpleUnaryOperator<IntView, SetView, OpSetSize> {
    using SimpleUnaryOperator<IntView, SetView, OpSetSize>::SimpleUnaryOperator;

    void reevaluate();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpSetSize& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_OPSETSIZE_H_ */
