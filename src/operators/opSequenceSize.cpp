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

void OpSequenceSize::copy(OpSequenceSize&) const {}

std::ostream& OpSequenceSize::dumpState(std::ostream& os) const {
    os << "OpSequenceSize: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}
string OpSequenceSize::getOpName() const { return "OpSequenceSize"; }
void OpSequenceSize::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto view = operand->getViewIfDefined();
    if (!view) {
        return;
    }
    sanityEqualsCheck((Int)view->numberElements(), value);
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
