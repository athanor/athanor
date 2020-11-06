#include "operators/opPartitionSize.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/partition.h"

using namespace std;

void OpPartitionSize::reevaluateImpl(PartitionView& operandView) {
    value = operandView.numberParts();
}
void OpPartitionSize::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    operand->updateVarViolations(vioContext, vioContainer);
}

void OpPartitionSize::copy(OpPartitionSize&) const {}

std::ostream& OpPartitionSize::dumpState(std::ostream& os) const {
    os << "OpPartitionSize: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}
string OpPartitionSize::getOpName() const { return "OpPartitionSize"; }
void OpPartitionSize::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto view = operand->getViewIfDefined();
    if (!view) {
        return;
    }
    sanityEqualsCheck((Int)view->numberParts(), value);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpPartitionSize> {
    static ExprRef<IntView> make(ExprRef<PartitionView> o);
};

ExprRef<IntView> OpMaker<OpPartitionSize>::make(ExprRef<PartitionView> o) {
    return make_shared<OpPartitionSize>(move(o));
}
