#include "operators/opSetNotEq.h"
#include <iostream>
#include <memory>
#include "types/set.h"

using namespace std;

inline void setViolation( bool trigger) {
    SetView& leftSetView = getView(left);
    SetView& rightSetView = getView(right);
    u_int64_t oldViolation = violation;
    violation =
        (leftSetView.cachedHashTotal == rightSetView.cachedHashTotal)
            ? 1
            : 0;
    if (trigger && violation != oldViolation) {
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(oldViolation); },
            triggers);
        visitTriggers(
            [&](auto& trigger) { trigger->valueChanged(violation); },
            triggers);
    }
}
void evaluate() {
    left->evaluate();
    right->evaluate();
    setViolation(op, false);
}

class OpSetNotEqTrigger : public SetTrigger {
   public:
    OpSetNotEq* op;

   public:
    OpSetNotEqTrigger(OpSetNotEq* op) : op(op) {}
    inline void valueRemoved(u_int64_t, u_int64_t) final {
        setViolation(*op, true);
    }

    inline void valueAdded(const AnyValRef&) final { setViolation(*op, true); }
    inline void setValueChanged(const SetView&) final {
        setViolation(*op, true);
    }
    inline void possibleMemberValueChange(u_int64_t, const AnyValRef&) final {}

    inline void memberValueChanged(u_int64_t, const AnyValRef&) final {
        setViolation(*op, true);
    }

    inline void iterHasNewValue(const SetValue&,
                                const ValRef<SetValue>&) final {
        assert(false);
    }
};

OpSetNotEq::OpSetNotEq(OpSetNotEq&& other)
    : BoolView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      trigger(std::move(other.trigger)) {
    setTriggerParent(this, trigger);
}

void startTriggering() {
    if (!trigger) {
        trigger = make_shared<OpSetNotEqTrigger>(this);
        addTrigger(left, trigger);
        addTrigger(right, trigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void stopTriggering() {
    if (trigger) {
        deleteTrigger(trigger);
        trigger = nullptr;
        left->stopTriggering();
        right->stopTriggering();
    }
}

void updateViolationDescription( u_int64_t,
                                ViolationDescription& vioDesc) {
    left->updateViolationDescription( violation, vioDesc);
    right->updateViolationDescription( violation, vioDesc);
}

std::shared_ptr<OpSetNotEq> deepCopyForUnroll(const OpSetNotEq&,
                                              const AnyIterRef&) {
    assert(false);
    abort();
}

std::ostream& dumpState(std::ostream& os, const OpSetNotEq&) {
    assert(false);
    return os;
}
