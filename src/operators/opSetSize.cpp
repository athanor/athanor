#include "operators/opSetSize.h"
#include <iostream>
#include <memory>
#include "types/set.h"

using namespace std;

void OpSetSize::evaluate() {
    operand->evaluate();
    value = operand->view().numberElements();
}

class OpSetSizeTrigger
    : public ChangeTriggerAdapter<SetTrigger, OpSetSizeTrigger> {
   public:
    OpSetSize* op;
    OpSetSizeTrigger(OpSetSize* op) : op(op) {}
    inline void adapterPossibleValueChange() {}
    inline void adapterValueChanged() {
        op->changeValue([&]() {
            op->value = op->operand->view().numberElements();
            return true;
        });
    }
};

OpSetSize::OpSetSize(OpSetSize&& other)
    : IntView(std::move(other)),
      operand(std::move(other.operand)),
      operandTrigger(std::move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}
void OpSetSize::startTriggering() {
    if (!operandTrigger) {
        operandTrigger = make_shared<OpSetSizeTrigger>(this);
        operand->addTrigger(operandTrigger);
        operand->startTriggering();
    }
}

void OpSetSize::stopTriggeringOnChildren() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
    }
}

void OpSetSize::stopTriggering() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
        operand->stopTriggering();
    }
}

void OpSetSize::updateViolationDescription(UInt parentViolation,
                                           ViolationDescription& vioDesc) {
    operand->updateViolationDescription(parentViolation, vioDesc);
}

ExprRef<IntView> IntView::deepCopySelfForUnroll(const ExprRef<IntView>&,ExprRef<IntView>ExprRef<IntView> OpSetSize::deepCopySelfForUnroll(const ExprRef<IntView>&,,
    const AnyIterRef& iterator) const {
    auto newOpSetSize = make_shared<OpSetSize>(
        operand->deepCopySelfForUnroll(operand, iterator));
    newOpSetSize->value = value;
    return newOpSetSize;
}

std::ostream& OpSetSize::dumpState(std::ostream& os) const {
    os << "OpSetSize: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}

void OpSetSize::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->operand = findAndReplace(operand, func);
}
