
#ifndef SRC_OPERATORS_OPSUM_H_
#define SRC_OPERATORS_OPSUM_H_
#include "operators/previousValueCache.h"
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
    PreviousValueCache<Int> cachedValues;
    OpSum(OpSum&&) = delete;
    void reevaluateImpl(SequenceView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpSum& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    bool optimiseImpl(const PathExtension&);
    void addSingleValue(Int exprValue);
    void removeSingleValue(Int exprValue);
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPSUM_H_ */
