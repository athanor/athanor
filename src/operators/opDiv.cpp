#include "operators/opDiv.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpDiv::reevaluate() {
    value = left->view().value / right->view().value;
    if (value < 0 && left->view().value % right->view().value != 0) {
        --value;
    }
}

void OpDiv::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    left->updateVarViolations(vioContext, vioContainer);
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
