
#ifndef SRC_OPERATORS_OPISDEFINED_H_
#define SRC_OPERATORS_OPISDEFINED_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
struct OpIsDefined;
template <>
struct OperatorTrates<OpIsDefined> {
    typedef SimpleUnaryTrigger<OpIsDefined, IntTrigger> OperandTrigger;
};
struct OpIsDefined
    : public SimpleUnaryOperator<BoolView, IntView, OpIsDefined> {
    using SimpleUnaryOperator<BoolView, IntView,
                              OpIsDefined>::SimpleUnaryOperator;

    void reevaluate();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription& vioDesc) final;
    void copy(OpIsDefined& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_OPISDEFINED_H_ */
