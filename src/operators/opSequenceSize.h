
#ifndef SRC_OPERATORS_OPSEQUENCESIZE_H_
#define SRC_OPERATORS_OPSEQUENCESIZE_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
#include "types/sequence.h"
struct OpSequenceSize;
template <>
struct OperatorTrates<OpSequenceSize> {
    typedef SimpleUnaryTrigger<OpSequenceSize, SequenceTrigger> OperandTrigger;
};
struct OpSequenceSize
    : public SimpleUnaryOperator<IntView, SequenceView, OpSequenceSize> {
    using SimpleUnaryOperator<IntView, SequenceView,
                              OpSequenceSize>::SimpleUnaryOperator;

    void reevaluateImpl(SequenceView& s);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpSequenceSize& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_OPSEQUENCESIZE_H_ */
