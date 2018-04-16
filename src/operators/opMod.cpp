#include "operators/opMod.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpMod::reevaluate() { value = left->view().value % right->view().value; }

void OpMod::updateViolationDescription(UInt parentViolation,
                                       ViolationDescription& vioDesc) {
    left->updateViolationDescription(parentViolation, vioDesc);
    right->updateViolationDescription(parentViolation, vioDesc);
}

void OpMod::copy(OpMod& newOp) const { newOp.value = value; }

ostream& OpMod::dumpState(ostream& os) const {
    os << "OpMod: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpMod> {
    ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<IntView> OpMaker<OpMod>::make(ExprRef<IntView> l, ExprRef<IntView> r) {
    return make_shared<OpMod>(move(l), move(r));
}
