#include "operators/opToInt.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/int.h"

using namespace std;

void OpToInt::reevaluateImpl(BoolView& operandView) {
    value = (operandView.violation == 0) ? 1 : 0;
}
void OpToInt::updateVarViolationsImpl(const ViolationContext& vioContext,
                                      ViolationContainer& vioContainer) {
    const IntViolationContext* intVioContextTest =
        dynamic_cast<const IntViolationContext*>(&vioContext);
    if (intVioContextTest) {
        if ((intVioContextTest->reason ==
                 IntViolationContext::Reason::TOO_LARGE &&
             value == 0) ||
            (intVioContextTest->reason ==
                 IntViolationContext::Reason::TOO_SMALL &&
             value == 1)) {
            return;
        }
    }
    operand->updateVarViolations(vioContext, vioContainer);
}

void OpToInt::copy(OpToInt& newOp) const { newOp.value = value; }

std::ostream& OpToInt::dumpState(std::ostream& os) const {
    os << "OpToInt: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpToInt> {
    static ExprRef<IntView> make(ExprRef<BoolView> o);
};

ExprRef<IntView> OpMaker<OpToInt>::make(ExprRef<BoolView> o) {
    return make_shared<OpToInt>(move(o));
}
