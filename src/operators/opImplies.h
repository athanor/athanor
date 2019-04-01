
#ifndef SRC_OPERATORS_OPIMPLIES_H_
#define SRC_OPERATORS_OPIMPLIES_H_

#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
struct OpImplies;
template <>
struct OperatorTrates<OpImplies> {
    typedef SimpleBinaryTrigger<OpImplies, BoolTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpImplies, BoolTrigger, false> RightTrigger;
};
struct OpImplies : public SimpleBinaryOperator<BoolView, BoolView, OpImplies> {
    using SimpleBinaryOperator<BoolView, BoolView,
                               OpImplies>::SimpleBinaryOperator;
    void reevaluateImpl(BoolView& leftView, BoolView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpImplies& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    inline ExprRef<BoolView> getCondition() { return left; }
};
#endif /* SRC_OPERATORS_OPIMPLIES_H_ */
