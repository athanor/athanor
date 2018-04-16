
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

    void reevaluate();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription& vioDesc) final;
    void copy(OpSum& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};

#endif /* SRC_OPERATORS_OPSUM_H_ */
