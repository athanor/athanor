#include "operators/opMod.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpMod::reevaluateImpl(IntView& leftView, IntView& rightView) {
    if (rightView.value == 0) {
        setDefined(false);
        return;
    }
    value = leftView.value % abs(rightView.value);
    if (value < 0) {
        value += abs(rightView.value);
    }
    if (rightView.value < 0 && value > 0) {
        value -= abs(rightView.value);
    }
    setDefined(true);
}

void OpMod::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    left->updateVarViolations(vioContext, vioContainer);
    right->updateVarViolations(vioContext, vioContainer);
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
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<IntView> OpMaker<OpMod>::make(ExprRef<IntView> l, ExprRef<IntView> r) {
    return make_shared<OpMod>(move(l), move(r));
}
