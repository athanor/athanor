#include "operators/opMSetSize.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/mSet.h"

using namespace std;

void OpMSetSize::reevaluateImpl(MSetView& operandView) {
    value = operandView.numberElements();
}
void OpMSetSize::updateVarViolationsImpl(const ViolationContext& vioContext,
                                         ViolationContainer& vioContainer) {
    operand->updateVarViolations(vioContext, vioContainer);
}

void OpMSetSize::copy(OpMSetSize&) const {}

std::ostream& OpMSetSize::dumpState(std::ostream& os) const {
    os << "OpMSetSize: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpMSetSize::getOpName() const { return "OpMSetSize"; }
void OpMSetSize::debugSanityCheckImpl() const {
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
struct OpMaker<OpMSetSize> {
    static ExprRef<IntView> make(ExprRef<MSetView> o);
};

ExprRef<IntView> OpMaker<OpMSetSize>::make(ExprRef<MSetView> o) {
    return make_shared<OpMSetSize>(move(o));
}
