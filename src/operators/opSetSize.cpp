#include "operators/opSetSize.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/set.h"

using namespace std;

void OpSetSize::reevaluate() { value = operand->view().numberElements(); }
void OpSetSize::updateViolationDescription(UInt parentViolation,
                                           ViolationDescription& vioDesc) {
    operand->updateViolationDescription(parentViolation, vioDesc);
}

void OpSetSize::copy(OpSetSize& newOp) const { newOp.value = value; }

std::ostream& OpSetSize::dumpState(std::ostream& os) const {
    os << "OpSetSize: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSetSize> {
    ExprRef<IntView> make(ExprRef<SetView> o);
};

ExprRef<IntView> OpMaker<OpSetSize>::make(ExprRef<SetView> o) {
    return make_shared<OpSetSize>(move(o));
}
