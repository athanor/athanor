
#ifndef SRC_OPERATORS_OPPROD_H_
#define SRC_OPERATORS_OPPROD_H_
#include "operators/simpleOperator.h"
#include "types/int.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"

struct OpProd;
template <>
struct OperatorTrates<OpProd> {
    class OperandsSequenceTrigger;
    typedef OperandsSequenceTrigger OperandTrigger;
};

struct OpProd : public SimpleUnaryOperator<IntView, SequenceView, OpProd> {
    using SimpleUnaryOperator<IntView, SequenceView,
                              OpProd>::SimpleUnaryOperator;
    void reevaluate();
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer& vioDesc) final;
    void copy(OpProd& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    bool optimiseImpl();
};

#endif /* SRC_OPERATORS_OPPROD_H_ */
