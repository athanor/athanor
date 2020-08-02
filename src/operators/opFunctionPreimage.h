
#ifndef SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_
#define SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_
#include "operators/simpleOperator.h"
#include "types/set.h"
#include "types/function.h"
template<typename OperandView>
struct OpFunctionPreimage;
template<typename OperandView>
struct OperatorTrates<OpFunctionPreimage<OperandView>> {
    struct LeftTrigger;
    struct RightTrigger;
};

template<typename OperandView>
struct OpFunctionPreimage
    : public SimpleBinaryOperator<SetView, OperandView, FunctioView,OpFunctionPreimage<OperandView>> {
    HashMap<HashType, UInt> hashExcesses;
    using SimpleBinaryOperator<SetView, OperandView, FunctioView,OpFunctionPreimage<OperandView>> ::SimpleBinaryOperator;

    void reevaluateImpl(MSetView& leftView, MSetView& rightView, bool, bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpFunctionPreimage& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_ */
