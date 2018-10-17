#include "operators/opIsDefined.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"

using namespace std;
template <typename View>
void OpIsDefined<View>::reevaluateImpl(View&) {
    this->violation = 0;
}
template <typename View>
void OpIsDefined<View>::updateVarViolationsImpl(
    const ViolationContext&, ViolationContainer& vioContainer) {
    if (this->violation > 0) {
        this->operand->updateVarViolations(this->violation, vioContainer);
    }
}
template <typename View>
void OpIsDefined<View>::copy(OpIsDefined& newOp) const {
    newOp.violation = this->violation;
}
template <typename View>
std::ostream& OpIsDefined<View>::dumpState(std::ostream& os) const {
    os << "OpIsDefined: violation=" << this->violation << "\noperand: ";
    this->operand->dumpState(os);
    return os;
}
template <typename View>
string OpIsDefined<View>::getOpName() const {
    return "OpIsDefined";
}

template <typename View>
void OpIsDefined<View>::debugSanityCheckImpl() const {
    this->operand->debugSanityCheck();
    bool isDefined = this->operand->getViewIfDefined().hasValue();
    if (isDefined) {
        sanityEqualsCheck(0, this->violation);
    } else {
        sanityLargeViolationCheck(this->violation);
    }
}
template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpIsDefined<View>> {
    static ExprRef<BoolView> make(ExprRef<View> o);
};
template <typename View>
ExprRef<BoolView> OpMaker<OpIsDefined<View>>::make(ExprRef<View> o) {
    return make_shared<OpIsDefined<View>>(move(o));
}

#define opIsDefinedInstantiators(name)       \
    template struct OpIsDefined<name##View>; \
    template struct OpMaker<OpIsDefined<name##View>>;

buildForAllTypes(opIsDefinedInstantiators, );
#undef opIsDefinedInstantiators
