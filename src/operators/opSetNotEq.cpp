#include "operators/opSetNotEq.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"

using namespace std;

void OpSetNotEq::reevaluate() {
    violation =
        (left->view().cachedHashTotal == right->view().cachedHashTotal) ? 1 : 0;
}

void OpSetNotEq::updateVarViolations(const ViolationContext&,
                                     ViolationContainer& vioDesc) {
    left->updateVarViolations(violation, vioDesc);
    right->updateVarViolations(violation, vioDesc);
}

void OpSetNotEq::copy(OpSetNotEq& newOp) const { newOp.violation = violation; }

std::ostream& OpSetNotEq::dumpState(std::ostream& os) const {
    os << "OpSetNotEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
    return os;
}

template <typename Op>
struct OpMaker;
template <>
struct OpMaker<OpSetNotEq> {
    static ExprRef<BoolView> make(ExprRef<SetView> l, ExprRef<SetView> r);
};

ExprRef<BoolView> OpMaker<OpSetNotEq>::make(ExprRef<SetView> l,
                                            ExprRef<SetView> r) {
    return make_shared<OpSetNotEq>(move(l), move(r));
}
