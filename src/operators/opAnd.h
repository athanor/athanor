
#ifndef SRC_OPERATORS_OPAND_H_
#define SRC_OPERATORS_OPAND_H_
#include <vector>
#include "operators/previousValueCache.h"
#include "operators/simpleOperator.h"
#include "types/bool.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"

struct OpAnd;
template <>
struct OperatorTrates<OpAnd> {
    class OperandsSequenceTrigger;
    typedef OperandsSequenceTrigger OperandTrigger;
};

struct OpAnd : public SimpleUnaryOperator<BoolView, SequenceView, OpAnd> {
    using SimpleUnaryOperator<BoolView, SequenceView,
                              OpAnd>::SimpleUnaryOperator;
    PreviousValueCache<UInt> cachedViolations;
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);

    inline OpAnd& operator=(const OpAnd& other) {
        operand = other.operand;
        violatingOperands = other.violatingOperands;
        return *this;
    }
    OpAnd(OpAnd&&) = delete;
    void reevaluateImpl(SequenceView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpAnd& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    bool optimiseImpl(const PathExtension&);
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPAND_H_ */
