
#ifndef SRC_OPERATORS_OPBOOLEQ_H_
#define SRC_OPERATORS_OPBOOLEQ_H_
#include "operators/definedVarHelper.h"
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"

struct OpBoolEq;
template <>
struct OperatorTrates<OpBoolEq> {
    typedef SimpleBinaryTrigger<OpBoolEq, BoolTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpBoolEq, BoolTrigger, false> RightTrigger;
};
struct OpBoolEq : public SimpleBinaryOperator<BoolView, BoolView, OpBoolEq> {
    using SimpleBinaryOperator<BoolView, BoolView,
                               OpBoolEq>::SimpleBinaryOperator;
    DefinesLock definesLock;
    DefinedVarTrigger<OpBoolEq>* definedVarTrigger = NULL;

    void reevaluateImpl(BoolView& leftView, BoolView& rightView,
                        bool leftChanged, bool rightChanged);
    void updateValue(BoolView& leftView, BoolView& rightView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpBoolEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    std::pair<bool, ExprRef<BoolView>> optimiseImpl(ExprRef<BoolView>& self,
                                                    PathExtension path) final;
};
#endif /* SRC_OPERATORS_OPBOOLEQ_H_ */
