#include "operators/opEnumEq.h"
#include "operators/definedVarHelper.hpp"
#include "operators/simpleOperator.hpp"
#include "types/enumVal.h"
using namespace std;
void OpEnumEq::reevaluateImpl(EnumView& leftView, EnumView& rightView,
                              bool leftChanged, bool rightChanged) {
    handledByForwardingValueChange(*this, leftView, rightView, leftChanged,
                                   rightChanged);
}

void OpEnumEq::updateValue(EnumView& leftView, EnumView& rightView) {
    changeValue([&]() {
        violation = (leftView.value == rightView.value) ? 0 : 1;
        return true;
    });
}

void OpEnumEq::updateVarViolationsImpl(const ViolationContext&,
                                       ViolationContainer& vioContainer) {
    if (violation == 0) {
        return;
    } else {
        left->updateVarViolations(violation, vioContainer);
        right->updateVarViolations(violation, vioContainer);
    }
}

void OpEnumEq::copy(OpEnumEq& newOp) const {
    newOp.violation = violation;
    if (definesLock.isDisabled()) {
        newOp.definesLock.disable();
    }
}
bool OpEnumEq::optimiseImpl(const PathExtension& path) {
    if (isSuitableForDefiningVars(path)) {
        return definesLock.reset();
    } else {
        definesLock.disable();
        return false;
    }
}

ostream& OpEnumEq::dumpState(ostream& os) const {
    os << "OpEnumEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os << ")";
}

string OpEnumEq::getOpName() const { return "OpEnumEq"; }
void OpEnumEq::debugSanityCheckImpl() const {
    left->debugSanityCheck();
    right->debugSanityCheck();
    auto leftOption = left->getViewIfDefined();
    auto rightOption = right->getViewIfDefined();
    if (!leftOption || !rightOption) {
        sanityLargeViolationCheck(violation);
        return;
    }
    auto& leftView = *leftOption;
    auto& rightView = *rightOption;
    UInt checkViolation = (leftView.value == rightView.value) ? 0 : 1;
    sanityEqualsCheck(checkViolation, violation);
}

template <typename>
struct OpMaker;
template <>
struct OpMaker<OpEnumEq> {
    static ExprRef<BoolView> make(ExprRef<EnumView> l, ExprRef<EnumView> r);
};

ExprRef<BoolView> OpMaker<OpEnumEq>::make(ExprRef<EnumView> l,
                                          ExprRef<EnumView> r) {
    return make_shared<OpEnumEq>(move(l), move(r));
}
