
#ifndef SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_
#define SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_
#include "operators/simpleOperator.h"
#include "types/function.h"
#include "types/set.h"
template <typename OperandView>
struct OpFunctionPreimage;
template <typename OperandView>
struct OperatorTrates<OpFunctionPreimage<OperandView>> {
    struct LeftTrigger;
    struct RightTrigger;
};

template <typename OperandView>
struct OpFunctionPreimage
    : public SimpleBinaryOperator<SetView, OperandView, FunctionView,
                                  OpFunctionPreimage<OperandView>> {
    lib::optional<HashType> cachedImageHash;
    HashMap<UInt, HashType> imageToPreimageHashMap;
    using SimpleBinaryOperator<
        SetView, OperandView, FunctionView,
        OpFunctionPreimage<OperandView>>::SimpleBinaryOperator;

    void reevaluateImpl(OperandView& leftView, FunctionView& rightView, bool,
                        bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpFunctionPreimage& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_ */
