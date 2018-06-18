
#ifndef SRC_OPERATORS_OPPOWER_H_
#define SRC_OPERATORS_OPPOWER_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
struct OpPower;
template <>
struct OperatorTrates<OpPower> {
    typedef SimpleBinaryTrigger<OpPower, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpPower, IntTrigger, false> RightTrigger;
};
struct OpPower : public SimpleBinaryOperator<IntView, IntView, OpPower> {
    using SimpleBinaryOperator<IntView, IntView, OpPower>::SimpleBinaryOperator;
    void reevaluate();
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer& vioDesc) final;
    void copy(OpPower& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPPOWER_H_ */