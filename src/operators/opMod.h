
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
struct OpMod : public SimpleBinaryOperator<IntView, IntView, OpMod> {
    using SimpleBinaryOperator<IntView, IntView, OpMod>::SimpleBinaryOperator;
    void reevaluate();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription& vioDesc) final;
    void copy(OpMod& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPMOD_H_ */
