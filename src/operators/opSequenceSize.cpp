#include "operators/opSequenceSize.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/sequence.h"

using namespace std;

void OpSequenceSize::reevaluateImpl(SequenceView& operandView) {
    value = operandView.numberElements();
}
void OpSequenceSize::updateVarViolationsImpl(const ViolationContext& vioContext,
                                             ViolationContainer& vioContainer) {
    operand->updateVarViolations(vioContext, vioContainer);
}

void OpSequenceSize::copy(OpSequenceSize& newOp) const { newOp.value = value; }

std::ostream& OpSequenceSize::dumpState(std::ostream& os) const {
    os << "OpSequenceSize: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSequenceSize> {
    static ExprRef<IntView> make(ExprRef<SequenceView> o);
};

ExprRef<IntView> OpMaker<OpSequenceSize>::make(ExprRef<SequenceView> o) {
    return make_shared<OpSequenceSize>(move(o));
}
