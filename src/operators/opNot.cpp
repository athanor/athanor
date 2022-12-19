#include "operators/opNot.h"

#include "operators/simpleOperator.hpp"
using namespace std;
void OpNot::reevaluateImpl(BoolView& view) {
    this->violation = (view.violation == 0) ? 1 : 0;
}

void OpNot::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    const BoolViolationContext* boolVioContextTest =
        dynamic_cast<const BoolViolationContext*>(&vioContext);
    bool alreadyNegated = boolVioContextTest && boolVioContextTest->negated;

    this->operand->updateVarViolations(
        BoolViolationContext(vioContext.parentViolation, !alreadyNegated),
        vioContainer);
}

void OpNot::copy(OpNot&) const {}

ostream& OpNot::dumpState(ostream& os) const {
    os << "OpNot: violation=" << violation << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpNot::getOpName() const { return "OpNot"; }
void OpNot::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto view = operand->getViewIfDefined();
    sanityCheck(view, "must always be defined.\n");
    if (view->violation == 0) {
        sanityEqualsCheck(1, violation);
    } else {
        sanityEqualsCheck(0, violation);
    }
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpNot> {
    static ExprRef<BoolView> make(ExprRef<BoolView> o);
};

ExprRef<BoolView> OpMaker<OpNot>::make(ExprRef<BoolView> o) {
    return make_shared<OpNot>(move(o));
}
