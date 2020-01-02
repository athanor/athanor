
#ifndef SRC_OPERATORS_OPSETINTERSECT_H_
#define SRC_OPERATORS_OPSETINTERSECT_H_
#include "operators/simpleOperator.h"
#include "types/bool.h"
#include "types/set.h"
struct OpSetIntersect;
template <>
struct OperatorTrates<OpSetIntersect> {
    template <bool isLeft>
    struct OperandTrigger;
    typedef OperandTrigger<true> LeftTrigger;
    typedef OperandTrigger<false> RightTrigger;
};
struct OpSetIntersect
    : public SimpleBinaryOperator<SetView, SetView, OpSetIntersect> {
    using SimpleBinaryOperator<SetView, SetView,
                               OpSetIntersect>::SimpleBinaryOperator;

    void reevaluateImpl(SetView& leftView, SetView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpSetIntersect& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    void valueRemoved(HashType hash);
    void valueAdded(HashType hash);
};
#endif /* SRC_OPERATORS_OPSETINTERSECT_H_ */
