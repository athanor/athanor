#include "operators/opMod.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;
void evaluate() {
    left->evaluate();
    right->evaluate();
    value =
        left->value % right->value;
}

struct OpMod::Trigger : public IntTrigger {
    OpMod* op;
    Trigger(OpMod* op) : op(op) {}
    inline void possibleValueChange(int64_t) final {}
    inline void valueChanged(int64_t) final {
        int64_t newValue = op->left->value %
                           op->right->value;
        if (newValue == op->value) {
            return;
        }
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->value); },
            op->triggers);
        op->value = newValue;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers);
    }

    inline void iterHasNewValue(const IntValue& oldValue,
                                const ValRef<IntValue>& newValue) final {
        possibleValueChange(oldValue.value);
        valueChanged(newValue->value);
    }
};

OpMod::OpMod(OpMod&& other)
    : IntView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      operandTrigger(std::move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}

void startTriggering() {
    if (!operandTrigger) {
        operandTrigger = make_shared<OpMod::Trigger>(this);
        addTrigger(left, operandTrigger);
        addTrigger(right, operandTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void stopTriggering() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
        left->stopTriggering();
        right->stopTriggering();
    }
}

void updateViolationDescription( u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(left, parentViolation, vioDesc);
    updateViolationDescription(right, parentViolation, vioDesc);
}

shared_ptr<OpMod> deepCopyForUnroll(
                                    const AnyIterRef& iterator) {
    auto newOpMod = make_shared<OpMod>(deepCopyForUnroll(left, iterator),
                                       deepCopyForUnroll(right, iterator));
    newOpMod->value = value;
    return newOpMod;
}

std::ostream& dumpState(std::ostream& os, ) {
    os << "OpMod: value=" << value << "\nleft: ";
    dumpState(os, left);
    os << "\nright: ";
    dumpState(os, right);
    return os;
}
