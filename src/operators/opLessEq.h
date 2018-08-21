
#ifndef SRC_OPERATORS_OPLESSEQ_H_
#define SRC_OPERATORS_OPLESSEQ_H_

#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
struct OpLessEq;
template <>
struct OperatorTrates<OpLessEq> {
    typedef SimpleBinaryTrigger<OpLessEq, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpLessEq, IntTrigger, false> RightTrigger;
};
struct OpLessEq : public SimpleBinaryOperator<BoolView, IntView, OpLessEq> {
    using SimpleBinaryOperator<BoolView, IntView,
                               OpLessEq>::SimpleBinaryOperator;

    void reevaluate();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpLessEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_OPLESSEQ_H_ */
