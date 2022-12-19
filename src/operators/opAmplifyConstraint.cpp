#include "operators/opAmplifyConstraint.h"

#include "operators/simpleOperator.hpp"
using namespace std;
void OpAmplifyConstraint::reevaluateImpl(BoolView& view) {
    if (view.violation != 0 && LARGE_VIOLATION / view.violation <= multiplier) {
        violation = LARGE_VIOLATION;
    } else {
        violation = view.violation * multiplier;
    }
}

void OpAmplifyConstraint::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    this->operand->updateVarViolations(vioContext, vioContainer);
}

void OpAmplifyConstraint::copy(OpAmplifyConstraint& other) const {
    other.multiplier = multiplier;
}

ostream& OpAmplifyConstraint::dumpState(ostream& os) const {
    os << "OpAmplifyConstraint: violation=" << violation << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpAmplifyConstraint::getOpName() const { return "OpAmplifyConstraint"; }
void OpAmplifyConstraint::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto view = operand->getViewIfDefined();
    sanityCheck(view, "must always be defined.\n");
    if (view->violation != 0 &&
        LARGE_VIOLATION / view->violation <= multiplier) {
        sanityEqualsCheck(LARGE_VIOLATION, violation);
    } else {
        sanityEqualsCheck(view->violation * multiplier, violation);
    }
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpAmplifyConstraint> {
    static ExprRef<BoolView> make(ExprRef<BoolView> o, UInt64 multiplier);
};

ExprRef<BoolView> OpMaker<OpAmplifyConstraint>::make(ExprRef<BoolView> o,
                                                     UInt64 multiplier) {
    return make_shared<OpAmplifyConstraint>(move(o), multiplier);
}
