
#ifndef SRC_OPERATORS_OPMOD_H_
#define SRC_OPERATORS_OPMOD_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
struct OpMod;
template <>
struct OperatorTrates<OpMod> {
    typedef SimpleBinaryTrigger<OpMod, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpMod, IntTrigger, false> RightTrigger;
};
struct OpMod : public SimpleBinaryOperator<IntView, IntView, IntView, OpMod> {
    using SimpleBinaryOperator<IntView, IntView, IntView,
                               OpMod>::SimpleBinaryOperator;
    void reevaluateImpl(IntView& leftView, IntView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpMod& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPMOD_H_ */
