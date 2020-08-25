
#ifndef SRC_OPERATORS_OPDIV_H_
#define SRC_OPERATORS_OPDIV_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
struct OpDiv;
template <>
struct OperatorTrates<OpDiv> {
    typedef SimpleBinaryTrigger<OpDiv, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpDiv, IntTrigger, false> RightTrigger;
};
struct OpDiv : public SimpleBinaryOperator<IntView, IntView, IntView, OpDiv> {
    using SimpleBinaryOperator<IntView, IntView, IntView,
                               OpDiv>::SimpleBinaryOperator;
    void reevaluateImpl(IntView& leftView, IntView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpDiv& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPDIV_H_ */
