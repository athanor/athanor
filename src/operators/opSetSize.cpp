#include "operators/opSetSize.h"
#include <iostream>
#include <memory>
#include "types/set.h"

using namespace std;

void evaluate(OpSetSize& op) {
    evaluate(op.operand);
    op.value = getView<SetView>(op.operand).memberHashes.size();
}

class OpSetSizeTrigger : public SetTrigger {
    friend OpSetSize;
    OpSetSize* op;

   public:
    OpSetSizeTrigger(OpSetSize* op) : op(op) {}
    inline void valueRemoved(const Value&) final {
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->value); },
            op->triggers, emptyEndOfTriggerQueue);
        --op->value;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers, emptyEndOfTriggerQueue);
    }

    inline void valueAdded(const Value&) final {
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->value); },
            op->triggers, emptyEndOfTriggerQueue);
        ++op->value;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers, emptyEndOfTriggerQueue);
    }

    inline void possibleValueChange(const Value&) {}
    inline void valueChanged(const Value&) {}
};

OpSetSize::OpSetSize(OpSetSize&& other)
    : IntView(std::move(other)),
      operand(std::move(other.operand)),
      operandTrigger(std::move(other.operandTrigger)) {
    operandTrigger->op = this;
}
void startTriggering(OpSetSize& op) {
    op.operandTrigger = make_shared<OpSetSizeTrigger>(&op);
    addTrigger<SetTrigger>(getView<SetView>(op.operand).triggers,
                           op.operandTrigger);
    startTriggering(op.operand);
}

void stopTriggering(OpSetSize& op) {
    deleteTrigger<SetTrigger>(getView<SetView>(op.operand).triggers,
                              op.operandTrigger);
    stopTriggering(op.operand);
}

void updateViolationDescription(const OpSetSize& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.operand, parentViolation, vioDesc);
}
