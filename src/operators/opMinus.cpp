#include "operators/opMinus.h"
#include "operators/simpleOperator.hpp"
using namespace std;

void OpMinus::reevaluate() { value = left->view().value - right->view().value; }

void OpMinus::updateVarViolations(UInt parentViolation,
                                         ViolationContainer& vioDesc) {
    left->updateVarViolations(parentViolation, vioDesc);
    right->updateVarViolations(parentViolation, vioDesc);
}

void OpMinus::copy(OpMinus& newOp) const { newOp.value = value; }

ostream& OpMinus::dumpState(ostream& os) const {
    os << "OpMinus: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpMinus> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<IntView> OpMaker<OpMinus>::make(ExprRef<IntView> l,
                                        ExprRef<IntView> r) {
    return make_shared<OpMinus>(move(l), move(r));
}
