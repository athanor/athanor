#include "operators/opIntEq.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;

inline void reevaluate(OpIntEq& op) {
    op.violation = std::abs(op.left->view().value - op.right->view().value);
}
void OpIntEq::evaluate() {
    left->evaluate();
    right->evaluate();
    reevaluate(*this);
}

OpIntEq::OpIntEq(OpIntEq&& other)
    : BoolView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      leftTrigger(std::move(other.leftTrigger)),
      rightTrigger(std::move(other.rightTrigger)) {
    setTriggerParent(this, leftTrigger, rightTrigger);
}

void OpIntEq::startTriggering() {
    if (!leftTrigger) {
        leftTrigger = make_shared<OpIntEq::LeftTrigger>(this);
        ;
        rightTrigger = make_shared<OpIntEq::RightTrigger>(this);
        ;
        left->addTrigger(leftTrigger);
        right->addTrigger(rightTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void OpIntEq::stopTriggeringOnChildren() {
    if (leftTrigger) {
        deleteTrigger(leftTrigger);
        leftTrigger = nullptr;
        deleteTrigger(rightTrigger);
        rightTrigger = nullptr;
    }
}

void OpIntEq::stopTriggering() {
    if (leftTrigger) {
        stopTriggeringOnChildren();
        left->stopTriggering();
        right->stopTriggering();
    }
}

void OpIntEq::updateViolationDescription(UInt, ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

ExprRef<BoolView> OpIntEq::deepCopySelfForUnroll(
    const ExprRef<BoolView>&, const AnyIterRef& iterator) const {
    auto newOpIntEq =
        make_shared<OpIntEq>(left->deepCopySelfForUnroll(left, iterator),
                             right->deepCopySelfForUnroll(right, iterator));
    newOpIntEq->violation = violation;
    return newOpIntEq;
}

std::ostream& OpIntEq::dumpState(std::ostream& os) const {
    os << "OpIntEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}
void OpIntEq::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->left = findAndReplace(left, func);
    this->right = findAndReplace(right, func);
}
