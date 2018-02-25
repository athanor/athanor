#include "operators/opSetSize.h"
#include <iostream>
#include <memory>
#include "types/set.h"

using namespace std;

void evaluate(OpSetSize& op) {
    evaluate(op.operand);
    op.value = getView<SetView>(op.operand).numberElements();
}

class OpSetSizeTrigger : public SetTrigger {
   public:
    OpSetSize* op;

   public:
    OpSetSizeTrigger(OpSetSize* op) : op(op) {}
    inline void valueRemoved(u_int64_t, u_int64_t) final {
        op->changeValue([&]() {
            --op->value;
            return true;
        });
    }

    inline void valueAdded(const AnyValRef&) final {
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
    inline void possibleMemberValueChange(u_int64_t, const AnyValRef&) final {}
    inline void memberValueChanged(u_int64_t, const AnyValRef&) final {}
    inline void iterHasNewValue(const SetValue&,
                                const ValRef<SetValue>& newValue) {
        int64_t newSize = newValue->numberElements();
        if (newSize == op->value) {
            return;
        }
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->value); },
            op->triggers);
        op->value = newSize;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers);
    }
};

OpSetSize::OpSetSize(OpSetSize&& other)
    : IntView(std::move(other)),
      operand(std::move(other.operand)),
      operandTrigger(std::move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}
void startTriggering(OpSetSize& op) {
    if (!op.operandTrigger) {
        op.operandTrigger = make_shared<OpSetSizeTrigger>(&op);
        addTrigger(op.operand, op.operandTrigger);
        startTriggering(op.operand);
    }
}

void stopTriggering(OpSetSize& op) {
    if (op.operandTrigger) {
        deleteTrigger(op.operandTrigger);
        op.operandTrigger = nullptr;
        stopTriggering(op.operand);
    }
}

void updateViolationDescription(const OpSetSize& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.operand, parentViolation, vioDesc);
}

std::shared_ptr<OpSetSize> deepCopyForUnroll(const OpSetSize& op,
                                             const AnyIterRef& iterator) {
    auto newOpSetSize =
        make_shared<OpSetSize>(deepCopyForUnroll(op.operand, iterator));
    newOpSetSize->value = op.value;
    return newOpSetSize;
}

std::ostream& dumpState(std::ostream& os, const OpSetSize& op) {
    os << "OpSetSize: value=" << op.value << "\noperand: ";
    dumpState(os, op.operand);
    return os;
}
