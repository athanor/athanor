#include "operators/opLessEq.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpLessEq::reevaluateImpl(IntView& leftView, IntView& rightView) {
    Int diff = rightView.value - leftView.value;
    violation = abs(min<Int>(diff, 0));
}

void OpLessEq::updateVarViolationsImpl(const ViolationContext&,
                                       ViolationContainer& vioContainer) {
    if (violation == 0) {
        return;
    } else if (allOperandsAreDefined()) {
        left->updateVarViolations(
            IntViolationContext(violation,
                                IntViolationContext::Reason::TOO_LARGE),
            vioContainer);
        right->updateVarViolations(
            IntViolationContext(violation,
                                IntViolationContext::Reason::TOO_SMALL),
            vioContainer);
    } else {
        left->updateVarViolations(violation, vioContainer);
        right->updateVarViolations(violation, vioContainer);
    }
}
void OpLessEq::copy(OpLessEq& newOp) const { newOp.violation = violation; }

ostream& OpLessEq::dumpState(ostream& os) const {
    os << "OpLessEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

string OpLessEq::getOpName() const { return "OpLessEq"; }
void OpLessEq::debugSanityCheckImpl() const {
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
    Int diff = rightView.value - leftView.value;
    UInt checkViolation = abs(min<Int>(diff, 0));

    sanityEqualsCheck(checkViolation, violation);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpLessEq> {
    static ExprRef<BoolView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<BoolView> OpMaker<OpLessEq>::make(ExprRef<IntView> l,
                                          ExprRef<IntView> r) {
    return make_shared<OpLessEq>(move(l), move(r));
}
