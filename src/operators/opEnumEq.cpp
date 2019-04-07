#include "operators/opEnumEq.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpEnumEq::reevaluateImpl(EnumView& leftView, EnumView& rightView) {
    violation = (leftView.value == rightView.value) ? 0 : 1;
}

void OpEnumEq::updateVarViolationsImpl(const ViolationContext&,
                                       ViolationContainer& vioContainer) {
    if (violation == 0) {
        return;
    } else {
        left->updateVarViolations(violation, vioContainer);
        right->updateVarViolations(violation, vioContainer);
    }
}

void OpEnumEq::copy(OpEnumEq& newOp) const { newOp.violation = violation; }

ostream& OpEnumEq::dumpState(ostream& os) const {
    os << "OpEnumEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

string OpEnumEq::getOpName() const { return "OpEnumEq"; }
void OpEnumEq::debugSanityCheckImpl() const {
    left->debugSanityCheck();
    right->debugSanityCheck();
    auto leftOption = left->getViewIfDefined();
    auto rightOption = right->getViewIfDefined();
    if (!leftOption || !rightOption) {
        sanityLargeViolationCheck(violation);
        return;
    }
    auto& leftView = *leftOption;
    auto& rightView = *rightOption;
    UInt checkViolation = (leftView.value == rightView.value) ? 0 : 1;
    sanityEqualsCheck(checkViolation, violation);
}

template <typename>
struct OpMaker;
template <>
struct OpMaker<OpEnumEq> {
    static ExprRef<BoolView> make(ExprRef<EnumView> l, ExprRef<EnumView> r);
};

ExprRef<BoolView> OpMaker<OpEnumEq>::make(ExprRef<EnumView> l,
                                          ExprRef<EnumView> r) {
    return make_shared<OpEnumEq>(move(l), move(r));
}
