
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
struct OpNotEq : public SimpleBinaryOperator<BoolView, OperandView, OperandView,
                                             OpNotEq<OperandView>> {
    using SimpleBinaryOperator<BoolView, OperandView, OperandView,
                               OpNotEq<OperandView>>::SimpleBinaryOperator;

    void reevaluateImpl(OperandView& leftView, OperandView& rightView, bool,
                        bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpNotEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPNOTEQ_H_ */
