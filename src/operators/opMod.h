
#ifndef SRC_OPERATORS_OPMOD_H_
#define SRC_OPERATORS_OPMOD_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
struct OpMod;
template <>
struct OperatorTrates<OpMod> {
    typedef SimpleBinaryTrigger<OpMod, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpMod, IntTrigger, false> RightTrigger;
};
struct OpMod : public SimpleBinaryOperator<IntView, IntView, OpMod> {
    using SimpleBinaryOperator<IntView, IntView, OpMod>::SimpleBinaryOperator;
    inline void reevaluate() {
        value = left->view().value % right->view().value;
    }

    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription& vioDesc) final {
        left->updateViolationDescription(parentViolation, vioDesc);
        right->updateViolationDescription(parentViolation, vioDesc);
    }

    inline void copy(OpMod& newOp) const { newOp.value = value; }

    inline std::ostream& dumpState(std::ostream& os) const final {
        os << "OpMod: value=" << value << "\nleft: ";
        left->dumpState(os);
        os << "\nright: ";
        right->dumpState(os);
        return os;
    }
};

#endif /* SRC_OPERATORS_OPMOD_H_ */
