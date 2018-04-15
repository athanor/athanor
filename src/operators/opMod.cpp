#include "operators/opMod.h"
#include "operators/simpleOperator.hpp"

void OpMod::reevaluate() { value = left->view().value % right->view().value; }

void OpMod::updateViolationDescription(UInt parentViolation,
                                       ViolationDescription& vioDesc) {
    left->updateViolationDescription(parentViolation, vioDesc);
    right->updateViolationDescription(parentViolation, vioDesc);
}

void OpMod::copy(OpMod& newOp) const { newOp.value = value; }

std::ostream& OpMod::dumpState(std::ostream& os) const final {
    os << "OpMod: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}
