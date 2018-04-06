#include "operators/opMod.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpMod::evaluate() {
    left->evaluate();
    right->evaluate();
    value = left->value % right->value;
}

struct OpMod::Trigger
    : public ChangeTriggerAdapter<IntTrigger, OpMod::Trigger> {
    OpMod* op;
    Trigger(OpMod* op) : op(op) {}
    inline void adapterPossibleValueChange() {}
    inline void adapterValueChanged() {
        op->changeValue([&]() {
            op->value = op->left->value % op->right->value;
            return true;
        });
    }
};

OpMod::OpMod(OpMod&& other)
    : IntView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      operandTrigger(std::move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}

void OpMod::startTriggering() {
    if (!operandTrigger) {
        operandTrigger = make_shared<OpMod::Trigger>(this);
        left->addTrigger( operandTrigger);
        right->addTrigger( operandTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void OpMod::stopTriggering() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
        left->stopTriggering();
        right->stopTriggering();
    }
}

void OpMod::updateViolationDescription(UInt parentViolation,
                                       ViolationDescription& vioDesc) {
    left->updateViolationDescription(parentViolation, vioDesc);
    right->updateViolationDescription(parentViolation, vioDesc);
}

ExprRef<IntView> OpMod::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpMod = make_shared<OpMod>(left->deepCopySelfForUnroll( iterator),
                                       right->deepCopySelfForUnroll( iterator));
    newOpMod->value = value;
    return ViewRef<IntView>(newOpMod);
}

std::ostream& OpMod::dumpState(std::ostream& os) const {
    os << "OpMod: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

void OpMod::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->left = findAndReplace(left, func);
    this->right = findAndReplace(right, func);
}
