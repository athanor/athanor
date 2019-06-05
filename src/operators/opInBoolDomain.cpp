#include "operators/opInBoolDomain.h"
#include <iostream>
#include <memory>
#include <tuple>
#include "operators/simpleOperator.hpp"

using namespace std;

void OpInDomain<BoolView>::reevaluateImpl(BoolView&) {}

void OpInDomain<BoolView>::updateVarViolationsImpl(const ViolationContext&,
                                                   ViolationContainer&) {}

void OpInDomain<BoolView>::copy(OpInDomain<BoolView>& newOp) const {
    newOp.domain = domain;
    newOp.violation = violation;
}

std::ostream& OpInDomain<BoolView>::dumpState(std::ostream& os) const {
    os << "OpInDomain<BoolView>: violation=" << violation << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpInDomain<BoolView>::getOpName() const {
    return "OpInDomain<BoolView>";
}
void OpInDomain<BoolView>::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    sanityCheck(operand->getViewIfDefined(),
                "bool operand must always be defined.");
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpInDomain<BoolView>> {
    static ExprRef<BoolView> make(shared_ptr<BoolDomain>, ExprRef<BoolView> o);
};

ExprRef<BoolView> OpMaker<OpInDomain<BoolView>>::make(
    shared_ptr<BoolDomain> domain, ExprRef<BoolView> o) {
    auto op = make_shared<OpInDomain<BoolView>>(move(o));
    op->domain = move(domain);
    return op;
}
