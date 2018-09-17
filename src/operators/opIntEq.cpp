#include "operators/opIntEq.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpIntEq::reevaluateImpl(IntView& leftView, IntView& rightView) {
    violation = abs(leftView.value - rightView.value);
}

void OpIntEq::updateVarViolationsImpl(const ViolationContext&,
                                      ViolationContainer& vioContainer) {
    if (violation == 0) {
        return;
    } else if (allOperandsAreDefined()) {
        Int diff = left->view().get().value - right->view().get().value;
        left->updateVarViolations(
            IntViolationContext(
                violation,
                ((diff > 0) ? IntViolationContext::Reason::TOO_LARGE
                            : IntViolationContext::Reason::TOO_SMALL)),
            vioContainer);
        right->updateVarViolations(
            IntViolationContext(
                violation,
                ((diff < 0) ? IntViolationContext::Reason::TOO_LARGE
                            : IntViolationContext::Reason::TOO_SMALL)),
            vioContainer);
    } else {
        left->updateVarViolations(violation, vioContainer);
        right->updateVarViolations(violation, vioContainer);
    }
}

void OpIntEq::copy(OpIntEq& newOp) const { newOp.violation = violation; }

ostream& OpIntEq::dumpState(ostream& os) const {
    os << "OpIntEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpIntEq> {
    static ExprRef<BoolView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<BoolView> OpMaker<OpIntEq>::make(ExprRef<IntView> l,
                                         ExprRef<IntView> r) {
    return make_shared<OpIntEq>(move(l), move(r));
}
