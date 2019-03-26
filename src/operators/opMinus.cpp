#include "operators/opMinus.h"
#include "operators/simpleOperator.hpp"
using namespace std;

void OpMinus::reevaluateImpl(IntView& leftView, IntView& rightView) {
    value = leftView.value - rightView.value;
}

void OpMinus::updateVarViolationsImpl(const ViolationContext& vioContext,
                                      ViolationContainer& vioContainer) {
    using Reason = IntViolationContext::Reason;
    auto* intVioContext = dynamic_cast<const IntViolationContext*>(&vioContext);

    left->updateVarViolations(vioContext, vioContainer);
    if (intVioContext) {
        right->updateVarViolations(
            IntViolationContext(intVioContext->parentViolation,
                                (intVioContext->reason == Reason::TOO_LARGE)
                                    ? Reason::TOO_SMALL
                                    : Reason::TOO_LARGE),
            vioContainer);
    } else {
        right->updateVarViolations(vioContext, vioContainer);
    }
}

void OpMinus::copy(OpMinus& newOp) const { newOp.value = value; }

ostream& OpMinus::dumpState(ostream& os) const {
    os << "OpMinus: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

string OpMinus::getOpName() const { return "OpMinus"; }
void OpMinus::debugSanityCheckImpl() const {
    left->debugSanityCheck();
    right->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto leftOption = left->getViewIfDefined();
    auto rightOption = right->getViewIfDefined();
    if (!leftOption || !rightOption) {
        return;
    }
    auto& leftView = *leftOption;
    auto& rightView = *rightOption;
    Int checkValue = leftView.value - rightView.value;
    sanityEqualsCheck(checkValue, value);
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
