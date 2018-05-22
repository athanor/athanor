#include "operators/opLess.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpLess::reevaluate() {
    Int diff = (right->view().value - 1) - left->view().value;
    violation = abs(min<Int>(diff, 0));
}

void OpLess::updateViolationDescription(UInt, ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

void OpLess::copy(OpLess& newOp) const { newOp.violation = violation; }

ostream& OpLess::dumpState(ostream& os) const {
    os << "OpLess: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpLess> {
    static ExprRef<BoolView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<BoolView> OpMaker<OpLess>::make(ExprRef<IntView> l,
                                        ExprRef<IntView> r) {
    return make_shared<OpLess>(move(l), move(r));
}
