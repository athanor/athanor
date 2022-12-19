
#ifndef SRC_OPERATORS_OPOR_H_
#define SRC_OPERATORS_OPOR_H_
#include <vector>

#include "operators/simpleOperator.h"
#include "types/bool.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"

struct OpOr;
template <>
struct OperatorTrates<OpOr> {
    class OperandsSequenceTrigger;
    typedef OperandsSequenceTrigger OperandTrigger;
};

struct OpOr : public SimpleUnaryOperator<BoolView, SequenceView, OpOr> {
    using SimpleUnaryOperator<BoolView, SequenceView,
                              OpOr>::SimpleUnaryOperator;
    FastIterableIntSet minViolationIndices = FastIterableIntSet(0, 0);
    OpOr(OpOr&& other) = delete;
    void reevaluateImpl(SequenceView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpOr& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;

    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    std::pair<bool, ExprRef<BoolView>> optimiseImpl(ExprRef<BoolView>&,
                                                    PathExtension path) final;
};

#endif /* SRC_OPERATORS_OPOR_H_ */
