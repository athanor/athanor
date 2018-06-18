
#ifndef SRC_OPERATORS_OPNOTEQ_H_
#define SRC_OPERATORS_OPNOTEQ_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/set.h"
template <typename OperandView>
struct OpNotEq;
template <typename OperandView>
struct OperatorTrates<OpNotEq<OperandView>> {
    typedef typename AssociatedTriggerType<OperandView>::type TriggerType;
    typedef SimpleBinaryTrigger<OpNotEq<OperandView>, TriggerType, true>
        LeftTrigger;
    typedef SimpleBinaryTrigger<OpNotEq<OperandView>, TriggerType, false>
        RightTrigger;
};

template <typename OperandView>
struct OpNotEq
    : public SimpleBinaryOperator<BoolView, OperandView, OpNotEq<OperandView>> {
    using SimpleBinaryOperator<BoolView, OperandView,
                               OpNotEq<OperandView>>::SimpleBinaryOperator;

    void reevaluate();
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer& vioDesc) final;
    void copy(OpNotEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_OPNOTEQ_H_ */
