
#ifndef SRC_OPERATORS_OPISDEFINED_H_
#define SRC_OPERATORS_OPISDEFINED_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
template <typename View>
struct OpIsDefined;
template <typename View>
struct OperatorTrates<OpIsDefined<View>> {
    typedef typename AssociatedTriggerType<View>::type TriggerType;
    typedef SimpleUnaryTrigger<OpIsDefined<View>, TriggerType> OperandTrigger;
};
template <typename View>
struct OpIsDefined
    : public SimpleUnaryOperator<BoolView, View, OpIsDefined<View>> {
    using SimpleUnaryOperator<BoolView, View,
                              OpIsDefined<View>>::SimpleUnaryOperator;

    void reevaluate();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpIsDefined& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_OPISDEFINED_H_ */
