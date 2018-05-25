#include "operators/opToInt.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/int.h"

using namespace std;

void OpToInt::reevaluate() { value = (operand->view().violation == 0) ? 1 : 0; }
void OpToInt::updateVarViolations(UInt parentViolation,
                                         ViolationContainer& vioDesc) {
    operand->updateVarViolations(parentViolation, vioDesc);
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
