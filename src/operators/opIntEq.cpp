#include "operators/opIntEq.h"
#include "operators/simpleOperator.hpp"

void OpIntEq::reevaluate() {
    violation = std::abs(left->view().value - right->view().value);
}

void OpIntEq::updateViolationDescription(UInt, ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

void OpIntEq::copy(OpIntEq& newOp) const { newOp.violation = violation; }

std::ostream& OpIntEq::dumpState(std::ostream& os) const {
    os << "OpIntEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}
