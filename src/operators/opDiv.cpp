#include "operators/opDiv.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpDiv::reevaluateImpl(IntView& leftView, IntView& rightView) {
    if (rightView.value == 0) {
        setDefined(false);
        return;
    }
    value = leftView.value / rightView.value;
    if (value < 0 && leftView.value % rightView.value != 0) {
        --value;
    }
    setDefined(true);
}

void OpDiv::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    left->updateVarViolations(vioContext, vioContainer);
    auto rightView = right->getViewIfDefined();
    if (rightView && rightView->value == 0) {
        right->updateVarViolations(LARGE_VIOLATION, vioContainer);
    }
    right->updateVarViolations(vioContext, vioContainer);
}

void OpDiv::copy(OpDiv& newOp) const { newOp.value = value; }

ostream& OpDiv::dumpState(ostream& os) const {
    os << "OpDiv: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpDiv> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<IntView> OpMaker<OpDiv>::make(ExprRef<IntView> l, ExprRef<IntView> r) {
    return make_shared<OpDiv>(move(l), move(r));
}
