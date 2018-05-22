#include "operators/opLessEq.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpLessEq::reevaluate() {
    Int diff = right->view().value - left->view().value;
    violation = abs(min<Int>(diff, 0));
}

void OpLessEq::updateViolationDescription(UInt, ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

void OpLessEq::copy(OpLessEq& newOp) const { newOp.violation = violation; }

ostream& OpLessEq::dumpState(ostream& os) const {
    os << "OpLessEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpLessEq> {
    static ExprRef<BoolView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<BoolView> OpMaker<OpLessEq>::make(ExprRef<IntView> l,
                                          ExprRef<IntView> r) {
    return make_shared<OpLessEq>(move(l), move(r));
}
