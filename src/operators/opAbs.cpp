#include "operators/opAbs.h"

#include <cmath>

#include "operators/simpleOperator.hpp"
using namespace std;
void OpAbs::reevaluateImpl(IntView& view) { this->value = llabs(view.value); }

void OpAbs::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    auto TOO_LARGE = IntViolationContext::Reason::TOO_LARGE;
    auto TOO_SMALL = IntViolationContext::Reason::TOO_SMALL;
    const IntViolationContext* intVioContextTest =
        dynamic_cast<const IntViolationContext*>(&vioContext);
    if (intVioContextTest) {
        this->operand->updateVarViolations(
            IntViolationContext(intVioContextTest->parentViolation,
                                (intVioContextTest->reason == TOO_LARGE)
                                    ? TOO_SMALL
                                    : TOO_LARGE),
            vioContainer);
    } else {
        this->operand->updateVarViolations(vioContext, vioContainer);
    }
}

void OpAbs::copy(OpAbs&) const {}

ostream& OpAbs::dumpState(ostream& os) const {
    os << "OpAbs: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpAbs::getOpName() const { return "OpAbs"; }
void OpAbs::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto view = operand->getViewIfDefined();
    if (!view) {
        return;
    }
    sanityEqualsCheck(llabs(view->value), value);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpAbs> {
    static ExprRef<IntView> make(ExprRef<IntView> o);
};

ExprRef<IntView> OpMaker<OpAbs>::make(ExprRef<IntView> o) {
    return make_shared<OpAbs>(move(o));
}
