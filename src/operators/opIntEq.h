
#ifndef SRC_OPERATORS_OPINTEQ_H_
#define SRC_OPERATORS_OPINTEQ_H_

#include "operators/definedVarHelper.h"
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
struct OpIntEq;
template <>
struct OperatorTrates<OpIntEq> {
    typedef SimpleBinaryTrigger<OpIntEq, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpIntEq, IntTrigger, false> RightTrigger;
};
struct OpIntEq : public SimpleBinaryOperator<BoolView, IntView, OpIntEq> {
    using SimpleBinaryOperator<BoolView, IntView,
                               OpIntEq>::SimpleBinaryOperator;
    DefinesLock definesLock;
    DefinedVarTrigger<OpIntEq>* definedVarTrigger = NULL;
    ~OpIntEq();
    void reevaluateImpl(IntView& leftView, IntView& rightView, bool, bool);
    void updateValue(IntView& leftView, IntView& rightView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpIntEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    std::pair<bool, ExprRef<BoolView>> optimiseImpl(ExprRef<BoolView>& self,
                                                    PathExtension path) final;
};
#endif /* SRC_OPERATORS_OPINTEQ_H_ */
