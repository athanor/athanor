
#ifndef SRC_OPERATORS_OPSETSIZE_H_
#define SRC_OPERATORS_OPSETSIZE_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
#include "types/intVal.h"
template <typename View>
struct OpInDomain;

template <>
struct OpInDomain<IntView>;

template <>
struct OperatorTrates<OpInDomain<IntView>> {
    typedef SimpleUnaryTrigger<OpInDomain<IntView>, IntTrigger> OperandTrigger;
};

template <>
struct OpInDomain<IntView>
    : public SimpleUnaryOperator<BoolView, IntView, OpInDomain<IntView>> {
    using SimpleUnaryOperator<BoolView, IntView,
                              OpInDomain<IntView>>::SimpleUnaryOperator;
    std::shared_ptr<IntDomain> domain;
    UInt closestBound = 0;
    IntViolationContext::Reason reason =
        IntViolationContext::Reason::TOO_SMALL;  // just giving any default
                                                 // value

    void reevaluateImpl(IntView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpInDomain<IntView>& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPSETSIZE_H_ */
