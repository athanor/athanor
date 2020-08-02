
#ifndef SRC_OPERATORS_OPLESS_H_
#define SRC_OPERATORS_OPLESS_H_

#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
struct OpLess;
template <>
struct OperatorTrates<OpLess> {
    typedef SimpleBinaryTrigger<OpLess, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpLess, IntTrigger, false> RightTrigger;
};
struct OpLess : public SimpleBinaryOperator<BoolView, IntView,IntView, OpLess> {
    using SimpleBinaryOperator<BoolView, IntView,IntView, OpLess>::SimpleBinaryOperator;

    void reevaluateImpl(IntView& leftView, IntView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpLess& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPLESS_H_ */
