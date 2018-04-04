#include "operators/opIntEq.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpIntEq::evaluate() {
    left->evaluate();
    right->evaluate();
    violation = std::abs(left->value - right->value);
}

struct OpIntEq::Trigger
    : public ChangeTriggerAdapter<IntTrigger, OpIntEq::Trigger> {
    OpIntEq* op;
    Trigger(OpIntEq* op) : op(op) {}
    inline void adapterPossibleValueChange() {}
    inline void adapterValueChanged() {
        op->changeValue([&]() {
            op->violation = std::abs(op->left->value - op->right->value);
            return true;
        });
    }
};

OpIntEq::OpIntEq(OpIntEq&& other)
    : BoolView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      operandTrigger(std::move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}

void OpIntEq::startTriggering() {
    if (!operandTrigger) {
        operandTrigger = make_shared<OpIntEq::Trigger>(this);
        addTrigger(left, operandTrigger);
        addTrigger(right, operandTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void OpIntEq::stopTriggering() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
        left->stopTriggering();
        right->stopTriggering();
    }
}

void OpIntEq::updateViolationDescription(UInt, ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

ExprRef<BoolView> OpIntEq::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpIntEq = make_shared<OpIntEq>(deepCopyForUnroll(left, iterator),
                                           deepCopyForUnroll(right, iterator));
    newOpIntEq->violation = violation;
    return ViewRef<BoolView>(newOpIntEq);
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
