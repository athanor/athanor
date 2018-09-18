#include "operators/opMinus.h"
#include "operators/simpleOperator.hpp"
using namespace std;

void OpMinus::reevaluateImpl(IntView& leftView, IntView& rightView) { value = leftView.value - rightView.value; }

void OpMinus::updateVarViolationsImpl(const ViolationContext& vioContext,
                                      ViolationContainer& vioContainer) {
    left->updateVarViolations(vioContext, vioContainer);
    right->updateVarViolations(vioContext, vioContainer);
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
