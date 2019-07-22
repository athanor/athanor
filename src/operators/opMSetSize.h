
#ifndef SRC_OPERATORS_OPMSETSIZE_H_
#define SRC_OPERATORS_OPMSETSIZE_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
#include "types/mSet.h"
struct OpMSetSize;
template <>
struct OperatorTrates<OpMSetSize> {
    typedef SimpleUnaryTrigger<OpMSetSize, MSetTrigger> OperandTrigger;
};
struct OpMSetSize : public SimpleUnaryOperator<IntView, MSetView, OpMSetSize> {
    using SimpleUnaryOperator<IntView, MSetView,
                              OpMSetSize>::SimpleUnaryOperator;

    void reevaluateImpl(MSetView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpMSetSize& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPMSETSIZE_H_ */
