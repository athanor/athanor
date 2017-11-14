#include "operators/opSetNotEq.h"
#include <iostream>
#include <memory>
#include "types/set.h"

using namespace std;

inline void setViolation(OpSetNotEq& op, bool trigger) {
    SetView& leftSetView = getView<SetView>(op.left);
    SetView& rightSetView = getView<SetView>(op.right);
    u_int64_t oldViolation = op.violation;
    op.violation =
        (leftSetView.cachedHashTotal == rightSetView.cachedHashTotal) ? 1 : 0;
    if (trigger && op.violation != oldViolation) {
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(oldViolation); },
            op.triggers, emptyEndOfTriggerQueue);
        visitTriggers(
            [&](auto& trigger) { trigger->valueChanged(op.violation); },
            op.triggers, emptyEndOfTriggerQueue);
    }
}

void evaluate(OpSetNotEq& op) {
    evaluate(op.left);
    evaluate(op.right);
    setViolation(op, false);
}

class OpSetNotEqTrigger : public SetTrigger {
    friend OpSetNotEq;
    OpSetNotEq* op;

   public:
    OpSetNotEqTrigger(OpSetNotEq* op) : op(op) {}
    inline void valueRemoved(const Value&) final { setViolation(*op, true); }

    inline void valueAdded(const Value&) final { setViolation(*op, true); }

    inline void possibleValueChange(const Value&) final {}

    inline void valueChanged(const Value&) final { setViolation(*op, true); }
};

OpSetNotEq::OpSetNotEq(OpSetNotEq&& other)
    : BoolView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      trigger(std::move(other.trigger)) {
    trigger->op = this;
}

void startTriggering(OpSetNotEq& op) {
    op.trigger = make_shared<OpSetNotEqTrigger>(&op);
    addTrigger<SetTrigger>(getView<SetView>(op.left).triggers, op.trigger);
    addTrigger<SetTrigger>(getView<SetView>(op.right).triggers, op.trigger);
    startTriggering(op.left);
    startTriggering(op.right);
}

void stopTriggering(OpSetNotEq& op) {
    deleteTrigger<SetTrigger>(op.trigger);
    stopTriggering(op.left);
    stopTriggering(op.right);
}

void updateViolationDescription(const OpSetNotEq& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.left, op.violation, vioDesc);
    updateViolationDescription(op.right, op.violation, vioDesc);
}
