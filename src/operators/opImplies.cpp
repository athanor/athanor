#include "operators/opImplies.h"
#include "operators/simpleOperator.hpp"
using namespace std;

void OpImplies::reevaluate() {
    if (left->view().violation != 0) {
        violation = 0;
    } else {
        violation = right->view().violation;
    }
}

void OpImplies::updateVarViolations(const ViolationContext&,
                                    ViolationContainer& vioDesc) {
    if (violation == 0) {
        return;
    } else {
        left->updateVarViolations(violation, vioDesc);
        right->updateVarViolations(violation, vioDesc);
    }
}
void OpImplies::copy(OpImplies& newOp) const { newOp.violation = violation; }

ostream& OpImplies::dumpState(ostream& os) const {
    os << "OpImplies: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpImplies> {
    static ExprRef<BoolView> make(ExprRef<BoolView> l, ExprRef<BoolView> r);
};

ExprRef<BoolView> OpMaker<OpImplies>::make(ExprRef<BoolView> l,
                                           ExprRef<BoolView> r) {
    return make_shared<OpImplies>(move(l), move(r));
}
