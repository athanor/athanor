
#ifndef SRC_OPERATORS_OPABS_H_
#define SRC_OPERATORS_OPABS_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
struct OpAbs;
template <>
struct OperatorTrates<OpAbs> {
    typedef SimpleUnaryTrigger<OpAbs, IntTrigger> OperandTrigger;
};

struct OpAbs : public SimpleUnaryOperator<IntView, IntView, OpAbs> {
    using SimpleUnaryOperator<IntView, IntView, OpAbs>::SimpleUnaryOperator;
    void reevaluateImpl(IntView& view);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpAbs& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPABS_H_ */
