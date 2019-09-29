#include "operators/opNotEq.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"

using namespace std;

template <typename OperandView>
void OpNotEq<OperandView>::reevaluateImpl(OperandView& leftView,
                                          OperandView& rightView, bool, bool) {
    this->violation =
        (getValueHash(leftView) == getValueHash(rightView)) ? 1 : 0;
}

template <typename OperandView>
void OpNotEq<OperandView>::updateVarViolationsImpl(
    const ViolationContext&, ViolationContainer& vioContainer) {
    this->left->updateVarViolations(this->violation, vioContainer);
    this->right->updateVarViolations(this->violation, vioContainer);
}

template <typename OperandView>
void OpNotEq<OperandView>::copy(OpNotEq&) const {}
template <typename OperandView>
std::ostream& OpNotEq<OperandView>::dumpState(std::ostream& os) const {
    os << "OpNotEq: violation=" << this->violation << "\nthis->left: ";
    this->left->dumpState(os);
    os << "\nthis->right: ";
    this->right->dumpState(os);
    return os;
    return os;
}
template <typename OperandView>
string OpNotEq<OperandView>::getOpName() const {
    return toString(
        "OpNotEq<",
        TypeAsString<typename AssociatedValueType<OperandView>::type>::value,
        ">");
}

template <typename OperandView>
void OpNotEq<OperandView>::debugSanityCheckImpl() const {
    this->left->debugSanityCheck();
    this->right->debugSanityCheck();
    auto leftOption = this->left->getViewIfDefined();
    auto rightOption = this->right->getViewIfDefined();
    if (!leftOption || !rightOption) {
        sanityLargeViolationCheck(this->violation);
        return;
    }
    auto& leftView = *leftOption;
    auto& rightView = *rightOption;
    UInt checkViolation =
        (getValueHash(leftView) == getValueHash(rightView)) ? 1 : 0;
    sanityEqualsCheck(checkViolation, this->violation);
}

template <typename Op>
struct OpMaker;
template <typename OperandView>
struct OpMaker<OpNotEq<OperandView>> {
    static ExprRef<BoolView> make(ExprRef<OperandView> l,
                                  ExprRef<OperandView> r);
};
template <typename OperandView>
ExprRef<BoolView> OpMaker<OpNotEq<OperandView>>::make(ExprRef<OperandView> l,
                                                      ExprRef<OperandView> r) {
    return make_shared<OpNotEq<OperandView>>(move(l), move(r));
}

#define opNotEqInstantiators(name)       \
    template struct OpNotEq<name##View>; \
    template struct OpMaker<OpNotEq<name##View>>;

buildForAllTypes(opNotEqInstantiators, );
#undef opNotEqInstantiators
