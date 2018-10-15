#include "operators/opDiv.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpDiv::reevaluateImpl(IntView& leftView, IntView& rightView) {
    if (rightView.value == 0) {
        setDefined(false);
        return;
    }
    value = leftView.value / rightView.value;
    if (value < 0 && leftView.value % rightView.value != 0) {
        --value;
    }
    setDefined(true);
}

void OpDiv::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    left->updateVarViolations(vioContext, vioContainer);
    auto rightView = right->getViewIfDefined();
    if (rightView && rightView->value == 0) {
        right->updateVarViolations(LARGE_VIOLATION, vioContainer);
    }
    right->updateVarViolations(vioContext, vioContainer);
}

void OpDiv::copy(OpDiv& newOp) const { newOp.value = value; }

ostream& OpDiv::dumpState(ostream& os) const {
    os << "OpDiv: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

string OpDiv::getOpName() const { return "OpDiv"; }

void OpDiv::debugSanityCheckImpl() const {
    left->debugSanityCheck();
    right->debugSanityCheck();
    auto leftViewOption = left->getViewIfDefined();
    auto rightViewOption = right->getViewIfDefined();
    if (!leftViewOption || !rightViewOption) {
        sanityCheck(
            !this->appearsDefined(),
            "operands are undefined, operator value should be undefined");
        return;
    }
    auto& leftView = *leftViewOption;
    auto& rightView = *rightViewOption;
    if (rightView.value == 0) {
        sanityCheck(!this->appearsDefined(),
                    "Should be undefined as right operand is 0");
        return;
    }
    sanityCheck(this->appearsDefined(),
                "Operands are defined and not dividing by 0, operator value "
                "should be defined.");
    Int checkValue = leftView.value / rightView.value;
    if (checkValue < 0 && leftView.value % rightView.value != 0) {
        --checkValue;
    }
    sanityCheck(value == checkValue,
                toString("operator value should be ", checkValue,
                         " but it is instead ", value));
}
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpDiv> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<IntView> OpMaker<OpDiv>::make(ExprRef<IntView> l, ExprRef<IntView> r) {
    return make_shared<OpDiv>(move(l), move(r));
}
