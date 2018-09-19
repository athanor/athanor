
#include "operators/opPower.h"
#include <cmath>
#include "operators/simpleOperator.hpp"
#include "utils/safePow.h"

using namespace std;

namespace {
UInt MAX_NUMBER_ALLOWED = (((UInt)1) << 61) - 10;
}  // namespace
void OpPower::reevaluateImpl(IntView& leftView, IntView& rightView) {
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
