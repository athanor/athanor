
#include "operators/opPower.h"
#include <cmath>
#include "operators/simpleOperator.hpp"
#include "utils/safePow.h"

using namespace std;

namespace {
UInt MAX_NUMBER_ALLOWED = (((UInt)1) << 61) - 10;
}  // namespace
void OpPower::reevaluateImpl(IntView& leftView, IntView& rightView, bool,
                             bool) {
    if (leftView.value == 0) {
        if (rightView.value == 0) {
            value = 1;
        } else if (rightView.value > 0) {
            value = 0;
        } else {
            setDefined(false);
        }
    } else {
        auto result =
            safePow<Int>(leftView.value, rightView.value, MAX_NUMBER_ALLOWED);
        if (result) {
            value = *result;
        } else {
            setDefined(false);
        }
    }
}

void OpPower::updateVarViolationsImpl(const ViolationContext& vioContext,
                                      ViolationContainer& vioContainer) {
    left->updateVarViolations(vioContext, vioContainer);
    right->updateVarViolations(vioContext, vioContainer);
}

void OpPower::copy(OpPower& newOp) const { newOp.value = value; }

ostream& OpPower::dumpState(ostream& os) const {
    os << "OpPower: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

string OpPower::getOpName() const { return "OpPower"; }
void OpPower::debugSanityCheckImpl() const {
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
    if (leftView.value == 0) {
        if (rightView.value < 0) {
            sanityCheck(!this->appearsDefined(),
                        "0 power negative number should be undefined, operator "
                        "should be undefined.");
            return;
        }
        sanityCheck(this->appearsDefined(), "operator should be defined.");
        if (rightView.value == 0) {
            sanityEqualsCheck(1, value);
        } else if (rightView.value > 0) {
            sanityEqualsCheck(0, value);
        }
    } else {
        auto result =
            safePow<Int>(leftView.value, rightView.value, MAX_NUMBER_ALLOWED);
        if (result) {
            sanityCheck(this->appearsDefined(), "operator should be defined.");
            sanityEqualsCheck(*result, value);
        } else {
            sanityCheck(!this->appearsDefined(),
                        "number too large, should be undefined.");
        }
    }
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpPower> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<IntView> OpMaker<OpPower>::make(ExprRef<IntView> l,
                                        ExprRef<IntView> r) {
    return make_shared<OpPower>(move(l), move(r));
}
