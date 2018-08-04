
#ifndef SRC_OPERATORS_OPALLDIFF_H_
#define SRC_OPERATORS_OPALLDIFF_H_
#include <vector>
#include "operators/simpleOperator.h"
#include "types/bool.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"
struct OpAllDiff;
template <>
struct OperatorTrates<OpAllDiff> {
    class OperandsSequenceTrigger;
    typedef OperandsSequenceTrigger OperandTrigger;
};

struct OpAllDiff
    : public SimpleUnaryOperator<BoolView, SequenceView, OpAllDiff> {
    struct Count {
        size_t value = 0;
    };

    using SimpleUnaryOperator<BoolView, SequenceView,
                              OpAllDiff>::SimpleUnaryOperator;
    std::unordered_map<HashType, size_t> hashCounts;
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    void reevaluate();
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer& vioDesc) final;
    void copy(OpAllDiff& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    bool optimiseImpl();
};

#endif /* SRC_OPERATORS_OPALLDIFF_H_ */
