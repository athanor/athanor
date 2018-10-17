#include "operators/opImplies.h"
#include "operators/simpleOperator.hpp"
using namespace std;

void OpImplies::reevaluateImpl(BoolView& leftView, BoolView& rightView) {
    if (leftView.violation != 0) {
        violation = 0;
    } else {
        violation = rightView.violation;
    }
}

void OpImplies::updateVarViolationsImpl(const ViolationContext&,
                                        ViolationContainer& vioContainer) {
    if (violation == 0) {
        return;
    } else {
        left->updateVarViolations(violation, vioContainer);
        right->updateVarViolations(violation, vioContainer);
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

string OpImplies::getOpName() const { return "OpImplies"; }
void OpImplies::debugSanityCheckImpl() const {
    left->debugSanityCheck();
    right->debugSanityCheck();
    auto leftViewOption = left->getViewIfDefined();
    auto rightViewOption = right->getViewIfDefined();
    sanityCheck(leftViewOption || rightViewOption,
                "boolean operands to this operator came back undefined even "
                "though they are boolean.");
    auto& leftView = *leftViewOption;
    auto& rightView = *rightViewOption;

    if (leftView.violation != 0) {
        sanityEqualsCheck(0, violation);
    } else {
        sanityEqualsCheck(rightView.violation, violation);
    }
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
