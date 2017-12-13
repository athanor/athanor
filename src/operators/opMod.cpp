#include "operators/opMod.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;
void evaluate(OpMod& op) {
    evaluate(op.left);
    evaluate(op.right);
    op.value =
        getView<IntView>(op.left).value % getView<IntView>(op.right).value;
}

struct OpMod::Trigger : public IntTrigger {
    OpMod* op;
    Trigger(OpMod* op) : op(op) {}
    inline void possibleValueChange(int64_t) final {}
    inline void valueChanged(int64_t) final {
        int64_t newValue = getView<IntView>(op->left).value %
                           getView<IntView>(op->right).value;
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

void startTriggering(OpMod& op) {
    op.operandTrigger = make_shared<OpMod::Trigger>(&op);
    addTrigger(op.left, op.operandTrigger);
    addTrigger(op.right, op.operandTrigger);
    startTriggering(op.left);
    startTriggering(op.right);
}

void stopTriggering(OpMod& op) {
    if (op.operandTrigger) {
        deleteTrigger(op.operandTrigger);
        op.operandTrigger = nullptr;
    }
    stopTriggering(op.left);
    stopTriggering(op.right);
}

void updateViolationDescription(const OpMod& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.left, parentViolation, vioDesc);
    updateViolationDescription(op.right, parentViolation, vioDesc);
}

shared_ptr<OpMod> deepCopyForUnroll(const OpMod& op,
                                    const AnyIterRef& iterator) {
    auto newOpMod = make_shared<OpMod>(deepCopyForUnroll(op.left, iterator),
                                       deepCopyForUnroll(op.right, iterator));
    newOpMod->value = op.value;
    return newOpMod;
}

std::ostream& dumpState(std::ostream& os, const OpMod& op) {
    os << "OpMod: value=" << op.value << "\nleft: ";
    dumpState(os, op.left);
    os << "\nright: ";
    dumpState(os, op.right);
    return os;
}
