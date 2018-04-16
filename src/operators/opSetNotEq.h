
#ifndef SRC_OPERATORS_OPSETNOTEQ_H_
#define SRC_OPERATORS_OPSETNOTEQ_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/set.h"
struct OpSetNotEq;
template <>
struct OperatorTrates<OpSetNotEq> {
    typedef SimpleBinaryTrigger<OpSetNotEq, SetTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpSetNotEq, SetTrigger, false> RightTrigger;
};
struct OpSetNotEq : public SimpleBinaryOperator<BoolView, SetView, OpSetNotEq> {
    using SimpleBinaryOperator<BoolView, SetView,
                               OpSetNotEq>::SimpleBinaryOperator;

    void reevaluate();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription& vioDesc) final;
    void copy(OpSetNotEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_OPSETNOTEQ_H_ */
