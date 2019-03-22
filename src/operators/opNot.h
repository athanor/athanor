
#ifndef SRC_OPERATORS_OPNOT_H_
#define SRC_OPERATORS_OPNOT_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
struct OpNot;
template <>
struct OperatorTrates<OpNot> {
    typedef SimpleUnaryTrigger<OpNot, BoolTrigger> OperandTrigger;
};

struct OpNot : public SimpleUnaryOperator<BoolView, BoolView, OpNot> {
    using SimpleUnaryOperator<BoolView, BoolView, OpNot>::SimpleUnaryOperator;
    void reevaluateImpl(BoolView& view);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpNot& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPNOT_H_ */
