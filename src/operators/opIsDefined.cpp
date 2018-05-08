#include "operators/opIsDefined.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"

using namespace std;

void OpIsDefined::reevaluate() { violation = 0; }
void OpIsDefined::updateViolationDescription(UInt,
                                             ViolationDescription& vioDesc) {
    if (violation > 0) {
        operand->updateViolationDescription(violation, vioDesc);
    }
}

void OpIsDefined::copy(OpIsDefined& newOp) const {
    newOp.violation = violation;
}

std::ostream& OpIsDefined::dumpState(std::ostream& os) const {
    os << "OpIsDefined: violation=" << violation << "\noperand: ";
    operand->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpIsDefined> {
    static ExprRef<BoolView> make(ExprRef<IntView> o);
};

ExprRef<BoolView> OpMaker<OpIsDefined>::make(ExprRef<IntView> o) {
    return make_shared<OpIsDefined>(move(o));
}
