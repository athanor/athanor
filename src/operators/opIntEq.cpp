#include "operators/opIntEq.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpIntEq::reevaluate() {
    violation = abs(left->view().value - right->view().value);
}

void OpIntEq::updateVarViolations(const ViolationContext&,
                                  ViolationContainer& vioDesc) {
    if (violation == 0) {
        return;
    } else if (allOperandsAreDefined()) {
        Int diff = left->view().value - right->view().value;
        left->updateVarViolations(
            IntViolationContext(
                violation,
                ((diff > 0) ? IntViolationContext::Reason::TOO_LARGE
                            : IntViolationContext::Reason::TOO_SMALL)),
            vioDesc);
        right->updateVarViolations(
            IntViolationContext(
                violation,
                ((diff < 0) ? IntViolationContext::Reason::TOO_LARGE
                            : IntViolationContext::Reason::TOO_SMALL)),
            vioDesc);
    } else {
        left->updateVarViolations(violation, vioDesc);
        right->updateVarViolations(violation, vioDesc);
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
