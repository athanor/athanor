
#ifndef SRC_OPERATORS_OPENUMEQ_H_
#define SRC_OPERATORS_OPENUMEQ_H_

#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/enum.h"
struct OpEnumEq;
template <>
struct OperatorTrates<OpEnumEq> {
    typedef SimpleBinaryTrigger<OpEnumEq, EnumTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpEnumEq, EnumTrigger, false> RightTrigger;
};

struct OpEnumEq : public SimpleBinaryOperator<BoolView, EnumView, OpEnumEq> {
    using SimpleBinaryOperator<BoolView, EnumView,
                               OpEnumEq>::SimpleBinaryOperator;

    void reevaluateImpl(EnumView& leftView, EnumView& rightView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpEnumEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPENUMEQ_H_ */
