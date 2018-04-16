#include "operators/opIntEq.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpIntEq::reevaluate() {
    violation = abs(left->view().value - right->view().value);
}

void OpIntEq::updateViolationDescription(UInt, ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
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
    ExprRef<BoolView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<BoolView> OpMaker<OpIntEq>::make(ExprRef<IntView> l,
                                         ExprRef<IntView> r) {
    return make_shared<OpIntEq>(move(l), move(r));
}
