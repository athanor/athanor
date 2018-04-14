
#ifndef SRC_OPERATORS_OPINTEQ_H_
#define SRC_OPERATORS_OPINTEQ_H_

#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/int.h"
struct OpIntEq;
template <>
struct OperatorTrates<OpIntEq> {
    typedef SimpleBinaryTrigger<OpIntEq, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpIntEq, IntTrigger, false> RightTrigger;
};
struct OpIntEq : public SimpleBinaryOperator<BoolView, IntView, OpIntEq> {
    using SimpleBinaryOperator<BoolView, IntView,
                               OpIntEq>::SimpleBinaryOperator;
    inline void reevaluate() {
        violation = std::abs(left->view().value - right->view().value);
    }

    inline void updateViolationDescription(
        UInt, ViolationDescription& vioDesc) final {
        left->updateViolationDescription(violation, vioDesc);
        right->updateViolationDescription(violation, vioDesc);
    }
    inline void copy(OpIntEq& newOp) const { newOp.violation = violation; }

    inline std::ostream& dumpState(std::ostream& os) const final {
        os << "OpIntEq: violation=" << violation << "\nleft: ";
        left->dumpState(os);
        os << "\nright: ";
        right->dumpState(os);
        return os;
    }
};
#endif /* SRC_OPERATORS_OPINTEQ_H_ */
