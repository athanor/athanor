#include "operators/opNegate.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpNegate::reevaluateImpl(IntView& view) { this->value = -view.value; }

void OpNegate::updateVarViolationsImpl(const ViolationContext& vioContext,
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

void OpNegate::copy(OpNegate&) const {}

ostream& OpNegate::dumpState(ostream& os) const {
    os << "OpNegate: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpNegate::getOpName() const { return "OpNegate"; }
void OpNegate::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto view = operand->getViewIfDefined();
    if (!view) {
        return;
    }
    sanityEqualsCheck(-view->value, value);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpNegate> {
    static ExprRef<IntView> make(ExprRef<IntView> o);
};

ExprRef<IntView> OpMaker<OpNegate>::make(ExprRef<IntView> o) {
    return make_shared<OpNegate>(move(o));
}
