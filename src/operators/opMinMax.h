
#ifndef SRC_OPERATORS_OPOR_H_
#define SRC_OPERATORS_OPOR_H_
#include <vector>
#include "operators/simpleOperator.h"
#include "types/int.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"
template <bool minMode>
struct OpMinMax;
template <bool minMode>
struct OperatorTrates<OpMinMax<minMode>> {
    class OperandsSequenceTrigger;
    typedef OperandsSequenceTrigger OperandTrigger;
};

template <bool minMode>
struct OpMinMax
    : public SimpleUnaryOperator<IntView, SequenceView, OpMinMax<minMode>> {
    using SimpleUnaryOperator<IntView, SequenceView,
                              OpMinMax<minMode>>::SimpleUnaryOperator;
    FastIterableIntSet minValueIndices = FastIterableIntSet(0, 0);

    void reevaluate();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription& vioDesc) final;
    void copy(OpMinMax& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;

    bool compare(Int u, Int v);
    bool optimiseImpl();
};
typedef OpMinMax<true> OpMin;
typedef OpMinMax<false> OpMax;
#endif /* SRC_OPERATORS_OPOR_H_ */
