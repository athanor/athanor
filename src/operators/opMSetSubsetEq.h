
#ifndef SRC_OPERATORS_OPMSETINTERSECT_H_
#define SRC_OPERATORS_OPMSETINTERSECT_H_
#include "operators/simpleOperator.h"
#include "types/bool.h"
#include "types/mSet.h"
struct OpMsetSubsetEq;
template <>
struct OperatorTrates<OpMsetSubsetEq> {
    struct LeftTrigger;
    struct RightTrigger;
};
struct OpMsetSubsetEq
    : public SimpleBinaryOperator<BoolView, MSetView, MSetView,OpMsetSubsetEq> {
    HashMap<HashType, UInt> hashExcesses;
    using SimpleBinaryOperator<BoolView, MSetView,MSetView,
                               OpMsetSubsetEq>::SimpleBinaryOperator;

    void reevaluateImpl(MSetView& leftView, MSetView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpMsetSubsetEq& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPMSETINTERSECT_H_ */
