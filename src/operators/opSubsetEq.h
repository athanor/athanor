
#ifndef SRC_OPERATORS_OPSETINTERSECT_H_
#define SRC_OPERATORS_OPSETINTERSECT_H_
#include "operators/simpleOperator.h"
#include "types/bool.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"
struct OpSubsetEq;
template <>
struct OperatorTrates<OpSubsetEq> {
    struct LeftTrigger;
    struct RightTrigger;
};
struct OpSubsetEq
    : public SimpleBinaryOperator<BoolView, SetView, SetView, OpSubsetEq> {
    FastIterableIntSet violatingMembers;

    using SimpleBinaryOperator<BoolView, SetView, SetView,
                               OpSubsetEq>::SimpleBinaryOperator;

    void reevaluateImpl(SetView& leftView, SetView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpSubsetEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPSETINTERSECT_H_ */
