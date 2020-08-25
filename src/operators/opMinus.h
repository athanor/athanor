
#ifndef SRC_OPERATORS_OPMINUS_H_
#define SRC_OPERATORS_OPMINUS_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
struct OpMinus;
template <>
struct OperatorTrates<OpMinus> {
    typedef SimpleBinaryTrigger<OpMinus, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpMinus, IntTrigger, false> RightTrigger;
};
struct OpMinus
    : public SimpleBinaryOperator<IntView, IntView, IntView, OpMinus> {
    using SimpleBinaryOperator<IntView, IntView, IntView,
                               OpMinus>::SimpleBinaryOperator;
    void reevaluateImpl(IntView& leftView, IntView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpMinus& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPMINUS_H_ */
