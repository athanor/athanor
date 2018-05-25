
#ifndef SRC_OPERATORS_OPAND_H_
#define SRC_OPERATORS_OPAND_H_
#include <vector>
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
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);

    inline OpAnd& operator=(const OpAnd& other) {
        operand = other.operand;
        violatingOperands = other.violatingOperands;
        return *this;
    }

    void reevaluate();
    void updateVarViolations(UInt parentViolation,
                                    ViolationContainer& vioDesc) final;
    void copy(OpAnd& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    bool optimiseImpl();
};

#endif /* SRC_OPERATORS_OPAND_H_ */
