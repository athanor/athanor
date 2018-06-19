#include "operators/opNotEq.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/allTypes.h"

using namespace std;

template <typename OperandView>
void OpNotEq<OperandView>::reevaluate() {
    this->violation =
        (getValueHash(this->left->view()) == getValueHash(this->right->view()))
            ? 1
            : 0;
}

template <typename OperandView>
void OpNotEq<OperandView>::updateVarViolations(const ViolationContext&,
                                               ViolationContainer& vioDesc) {
    this->left->updateVarViolations(this->violation, vioDesc);
    this->right->updateVarViolations(this->violation, vioDesc);
}

template <typename OperandView>
void OpNotEq<OperandView>::copy(OpNotEq& newOp) const {
    newOp.violation = this->violation;
}
template <typename OperandView>
std::ostream& OpNotEq<OperandView>::dumpState(std::ostream& os) const {
    os << "OpNotEq: violation=" << this->violation << "\nthis->left: ";
    this->left->dumpState(os);
    os << "\nthis->right: ";
    this->right->dumpState(os);
    return os;
    return os;
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