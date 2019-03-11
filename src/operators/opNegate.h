
#ifndef SRC_OPERATORS_OPNEGATE_H_
#define SRC_OPERATORS_OPNEGATE_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
struct OpNegate;
template <>
struct OperatorTrates<OpNegate> {
    typedef SimpleUnaryTrigger<OpNegate, IntTrigger> OperandTrigger;
};

struct OpNegate : public SimpleUnaryOperator<IntView, IntView, OpNegate> {
    using SimpleUnaryOperator<IntView, IntView, OpNegate>::SimpleUnaryOperator;
    void reevaluateImpl(IntView& view);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpNegate& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPNEGATE_H_ */
