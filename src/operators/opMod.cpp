#include "operators/opMod.h"
#include "operators/simpleOperator.hpp"
using namespace std;
void OpMod::reevaluateImpl(IntView& leftView, IntView& rightView) {
    if (rightView.value == 0) {
        setDefined(false);
        return;
    }
    value = leftView.value % abs(rightView.value);
    if (value < 0) {
        value += abs(rightView.value);
    }
    if (rightView.value < 0 && value > 0) {
        value -= abs(rightView.value);
    }
    setDefined(true);
}

void OpMod::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    left->updateVarViolations(vioContext, vioContainer);
    right->updateVarViolations(vioContext, vioContainer);
}

void OpMod::copy(OpMod& newOp) const { newOp.value = value; }

ostream& OpMod::dumpState(ostream& os) const {
    os << "OpMod: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

string OpMod::getOpName() const { return "OpMod"; }
void OpMod::debugSanityCheckImpl() const {
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
                "Operands are defined and not moddingby 0, operator value "
                "should be defined.");
    Int checkValue = leftView.value % abs(rightView.value);
    if (checkValue < 0) {
        checkValue += abs(rightView.value);
    }
    if (rightView.value < 0 && checkValue > 0) {
        checkValue -= abs(rightView.value);
    }
    sanityEqualsCheck(checkValue, value);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpMod> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<IntView> OpMaker<OpMod>::make(ExprRef<IntView> l, ExprRef<IntView> r) {
    return make_shared<OpMod>(move(l), move(r));
}
