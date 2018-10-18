
#ifndef SRC_OPERATORS_OPPOWERSET_H_
#define SRC_OPERATORS_OPPOWERSET_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/set.h"
struct OpPowerSet;
template <>
struct OperatorTrates<OpPowerSet> {
    struct OperandTrigger;
};
struct OpPowerSet : public SimpleUnaryOperator<SetView, SetView, OpPowerSet> {
    int sizeLimit;
    OpPowerSet(ExprRef<SetView> operand, int sizeLimit = -1)
        : SimpleUnaryOperator<SetView, SetView, OpPowerSet>(std::move(operand)),
          sizeLimit(sizeLimit) {}
    void setInnerType() { members.emplace<ExprRefVec<SetView>>(); }
    void reevaluateImpl(SetView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpPowerSet& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;

};
#endif /* SRC_OPERATORS_OPPOWERSET_H_ */
