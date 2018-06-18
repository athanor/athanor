#include "operators/opPower.h"
#include "operators/simpleOperator.hpp"
using namespace std;
#include <cmath>
void OpPower::reevaluate() {
    value = pow(left->view().value, right->view().value);
}

void OpPower::updateVarViolations(const ViolationContext& vioContext,
                                  ViolationContainer& vioDesc) {
    left->updateVarViolations(vioContext, vioDesc);
    right->updateVarViolations(vioContext, vioDesc);
}

void OpPower::copy(OpPower& newOp) const { newOp.value = value; }

ostream& OpPower::dumpState(ostream& os) const {
    os << "OpPower: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpPower> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<IntView> OpMaker<OpPower>::make(ExprRef<IntView> l,
                                        ExprRef<IntView> r) {
    return make_shared<OpPower>(move(l), move(r));
}
