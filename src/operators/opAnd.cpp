#include "operators/opAnd.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "operators/flatten.h"
#include "operators/previousValueCache.h"
#include "operators/simpleOperator.hpp"
#include "types/boolVal.h"
#include "types/sequenceVal.h"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OperatorTrates<OpAnd>::OperandsSequenceTrigger;

void OpAnd::reevaluateImpl(SequenceView& operandView) {
    violation = 0;
    cachedViolations.clear();
    for (size_t i = 0; i < operandView.numberElements(); ++i) {
        auto& operandChild = operand->view()->getMembers<BoolView>()[i];
        UInt operandViolation = operandChild->view()->violation;
        cachedViolations.insert(i, operandViolation);
        if (operandViolation > 0) {
            violatingOperands.insert(i);
        }
        violation += operandViolation;
    }
}

void OpAnd::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    auto* boolVioContextTest =
        dynamic_cast<const BoolViolationContext*>(&vioContext);
    bool negated = boolVioContextTest && boolVioContextTest->negated;
    if (negated) {
        if (violation == 0) {
            for (auto& operand : operand->view()->getMembers<BoolView>()) {
                operand->updateVarViolations(vioContext, vioContainer);
            }
        }
        return;
    }
    for (size_t violatingOperandIndex : violatingOperands) {
        operand->view()
            .get()
            .getMembers<BoolView>()[violatingOperandIndex]
            ->updateVarViolations(violation, vioContainer);
    }
}

void OpAnd::copy(OpAnd&) const {}

std::ostream& OpAnd::dumpState(std::ostream& os) const {
    os << "OpAnd: violation=" << violation << endl;
    vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                         violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
    return operand->dumpState(os) << ")";
}

std::pair<bool, ExprRef<BoolView>> OpAnd::optimiseImpl(ExprRef<BoolView>& self,
                                                       PathExtension path) {
    auto boolOpPair = standardOptimise(self, path);
    boolOpPair.first |= flatten<BoolView>(*(boolOpPair.second));
    return boolOpPair;
}

string OpAnd::getOpName() const { return "OpAnd"; }
void OpAnd::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    auto view = operand->getViewIfDefined();
    if (!view) {
        sanityLargeViolationCheck(violation);
        return;
    }
    auto& operandView = *view;
    UInt calcViolation = 0;
    for (auto& member : operandView.getMembers<BoolView>()) {
        auto memberView = member->getViewIfDefined();
        sanityCheck(memberView,
                    "View should not be undefined, it is a bool view.");
        calcViolation += memberView->violation;
    }
    sanityEqualsCheck(calcViolation, violation);
    ;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpAnd> {
    static ExprRef<BoolView> make(
        ExprRef<SequenceView>,
        const shared_ptr<SequenceDomain>& operandDomain = nullptr);
};

ExprRef<BoolView> OpMaker<OpAnd>::make(
    ExprRef<SequenceView> o, const shared_ptr<SequenceDomain>& operandDomain) {
    if (operandDomain &&
        lib::get_if<shared_ptr<EmptyDomain>>(&operandDomain->inner)) {
        auto val = ::make<BoolValue>();
        val->violation = 0;
        val->setConstant(true);
        return val.asExpr();
    }
    return make_shared<OpAnd>(move(o));
}

template struct SimpleUnaryOperator<BoolView, SequenceView, OpAnd>;
