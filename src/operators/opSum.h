
#ifndef SRC_OPERATORS_OPSUM_H_
#define SRC_OPERATORS_OPSUM_H_
#include "operators/simpleOperator.h"
#include "types/int.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"

struct OpSum;
template <>
struct OperatorTrates<OpSum> {
    class OperandsSequenceTrigger;
    typedef OperandsSequenceTrigger OperandTrigger;
};

struct OpSum : public SimpleUnaryOperator<IntView, SequenceView, OpSum> {
    using SimpleUnaryOperator<IntView, SequenceView,
                              OpSum>::SimpleUnaryOperator;
    bool evaluationComplete = false;
    OpSum(OpSum&& other);
    void reevaluate();
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer& vioDesc) final;
    void copy(OpSum& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    bool optimiseImpl();
};

#endif /* SRC_OPERATORS_OPSUM_H_ */
