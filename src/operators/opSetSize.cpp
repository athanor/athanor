#include "operators/opSetSize.h"

#include <iostream>
#include <memory>

#include "operators/opPartitionParts.h"
#include "operators/simpleOperator.hpp"
#include "types/set.h"

using namespace std;
template <typename>
struct OpMaker;
struct OpPartitionSize;
template <>
struct OpMaker<OpPartitionSize> {
    static ExprRef<IntView> make(ExprRef<PartitionView> o);
};

void OpSetSize::reevaluateImpl(SetView& operandView) {
    value = operandView.numberElements();
}
void OpSetSize::updateVarViolationsImpl(const ViolationContext& vioContext,
                                        ViolationContainer& vioContainer) {
    operand->updateVarViolations(vioContext, vioContainer);
}

void OpSetSize::copy(OpSetSize&) const {}

std::ostream& OpSetSize::dumpState(std::ostream& os) const {
    os << "OpSetSize: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpSetSize::getOpName() const { return "OpSetSize"; }
void OpSetSize::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto view = operand->getViewIfDefined();
    if (!view) {
        return;
    }
    sanityEqualsCheck((Int)view->numberElements(), value);
}

std::pair<bool, ExprRef<IntView>> OpSetSize::optimiseImpl(
    ExprRef<IntView>& self, PathExtension path) {
    auto boolOpPair = standardOptimise(self, path);
    auto partitionPartsTest =
        getAs<OpPartitionParts>(boolOpPair.second->operand);
    if (partitionPartsTest) {
        debug_log(
            "Optimise: replacing size of parts(partition) with "
            "numberParts(partition)");
        return make_pair(
            true, OpMaker<OpPartitionSize>::make(partitionPartsTest->operand));
    }
    return boolOpPair;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSetSize> {
    static ExprRef<IntView> make(ExprRef<SetView> o);
};

ExprRef<IntView> OpMaker<OpSetSize>::make(ExprRef<SetView> o) {
    return make_shared<OpSetSize>(move(o));
}
