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
   public:
    OpSetSize* op;

   public:
    OpSetSizeTrigger(OpSetSize* op) : op(op) {}
    inline void valueRemoved(const AnyValRef&) final {
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->value); },
            op->triggers);
        --op->value;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers);
    }

    inline void valueAdded(const AnyValRef&) final {
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->value); },
            op->triggers);
        ++op->value;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers);
    }

    inline void possibleValueChange(const AnyValRef&) {}
    inline void valueChanged(const AnyValRef&) {}
};

OpSetSize::OpSetSize(OpSetSize&& other)
    : IntView(std::move(other)),
      operand(std::move(other.operand)),
      operandTrigger(std::move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}
void startTriggering(OpSetSize& op) {
    op.operandTrigger = make_shared<OpSetSizeTrigger>(&op);
    addTrigger<SetTrigger>(getView<SetView>(op.operand).triggers,
                           op.operandTrigger);
    startTriggering(op.operand);
}

void stopTriggering(OpSetSize& op) {
    if (op.operandTrigger) {
        deleteTrigger(op.operandTrigger);
    }
    stopTriggering(op.operand);
}

void updateViolationDescription(const OpSetSize& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.operand, parentViolation, vioDesc);
}

std::shared_ptr<OpSetSize> deepCopyForUnroll(const OpSetSize&,
                                             const AnyIterRef&) {
    assert(false);
    abort();
}
