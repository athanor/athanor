
#ifndef SRC_OPERATORS_OPPARTITIONSIZE_H_
#define SRC_OPERATORS_OPPARTITIONSIZE_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
#include "types/partition.h"
struct OpPartitionSize;
template <>
struct OperatorTrates<OpPartitionSize> {
    typedef SimpleUnaryTrigger<OpPartitionSize, PartitionTrigger> OperandTrigger;
};
struct OpPartitionSize
    : public SimpleUnaryOperator<IntView, PartitionView, OpPartitionSize> {
    using SimpleUnaryOperator<IntView, PartitionView,
                              OpPartitionSize>::SimpleUnaryOperator;

    void reevaluateImpl(PartitionView& s);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpPartitionSize& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPPARTITIONSIZE_H_ */
