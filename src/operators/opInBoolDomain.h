
#ifndef SRC_OPERATORS_OPInBoolDOMAIN_H_
#define SRC_OPERATORS_OPInBoolDOMAIN_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"

#include "types/boolVal.h"
template <typename View>
struct OpInDomain;

template <>
struct OpInDomain<BoolView>;

template <>
struct OperatorTrates<OpInDomain<BoolView>> {
    typedef SimpleUnaryTrigger<OpInDomain<BoolView>, BoolTrigger>
        OperandTrigger;
};

template <>
struct OpInDomain<BoolView>
    : public SimpleUnaryOperator<BoolView, BoolView, OpInDomain<BoolView>> {
    using SimpleUnaryOperator<BoolView, BoolView,
                              OpInDomain<BoolView>>::SimpleUnaryOperator;
    std::shared_ptr<BoolDomain> domain;

    void reevaluateImpl(BoolView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpInDomain<BoolView>& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPInBoolDOMAIN_H_ */
