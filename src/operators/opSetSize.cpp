#include "operators/opSetSize.h"
#include <iostream>
#include <memory>
#include "types/set.h"

using namespace std;

void OpSetSize::evaluate() {
    operand->evaluate();
    value = operand->numberElements();
}

class OpSetSizeTrigger : public SetTrigger {
   public:
    OpSetSize* op;

   public:
    OpSetSizeTrigger(OpSetSize* op) : op(op) {}
    inline void valueRemoved(UInt, HashType) final {
        op->changeValue([&]() {
            --op->value;
            return true;
        });
    }

    inline void valueAdded(const AnyExprRef&) final {
        op->changeValue([&]() {
            ++op->value;
            return true;
        });
    }
    inline void setValueChanged(const SetView& newValue) final {
        op->changeValue([&]() {
            op->value = newValue.numberElements();
            return true;
        });
    }
    inline void possibleMemberValueChange(UInt, const AnyExprRef&) final {}
    inline void memberValueChanged(UInt, const AnyExprRef&) final {}
    inline void iterHasNewValue(const SetView&,
                                const ExprRef<SetView>& newValue) {
        setValueChanged(*newValue);
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
        addTrigger(operand, operandTrigger);
        operand->startTriggering();
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

ExprRef<IntView> OpSetSize::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpSetSize =
        make_shared<OpSetSize>(deepCopyForUnroll(operand, iterator));
    newOpSetSize->value = value;
    return ViewRef<IntView>(newOpSetSize);
}

std::ostream& OpSetSize::dumpState(std::ostream& os) const {
    os << "OpSetSize: value=" << value << "\noperand: ";
    operand->dumpState(os);
    return os;
}
