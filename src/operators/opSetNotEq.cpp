#include "operators/opSetNotEq.h"
#include <iostream>
#include <memory>
#include "types/set.h"

using namespace std;

inline void setViolation(OpSetNotEq& op, bool trigger) {
    UInt newViolation =
        (op.left->cachedHashTotal == op.right->cachedHashTotal) ? 1 : 0;
    if (trigger) {
        op.changeValue([&]() {
            op.violation = newViolation;
            return true;
        });
    } else {
        op.violation = newViolation;
    }
}

void OpSetNotEq::evaluate() {
    left->evaluate();
    right->evaluate();
    setViolation(*this, false);
}

class OpSetNotEqTrigger
    : public ChangeTriggerAdapter<SetTrigger, OpSetNotEqTrigger> {
   public:
    OpSetNotEq* op;
    OpSetNotEqTrigger(OpSetNotEq* op) : op(op) {}
    inline void adapterPossibleValueChange() {}
    inline void adapterValueChanged() { setViolation(*op, true); }
};

OpSetNotEq::OpSetNotEq(OpSetNotEq&& other)
    : BoolView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      trigger(std::move(other.trigger)) {
    setTriggerParent(this, trigger);
}

void OpSetNotEq::startTriggering() {
    if (!trigger) {
        trigger = make_shared<OpSetNotEqTrigger>(this);
        addTrigger(left, trigger);
        addTrigger(right, trigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void OpSetNotEq::stopTriggering() {
    if (trigger) {
        deleteTrigger(trigger);
        trigger = nullptr;
        left->stopTriggering();
        right->stopTriggering();
    }
}

void OpSetNotEq::updateViolationDescription(UInt,
                                            ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

ExprRef<BoolView> OpSetNotEq::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpSetNotEq = make_shared<OpSetNotEq>(
        deepCopyForUnroll(left, iterator), deepCopyForUnroll(right, iterator));
    newOpSetNotEq->violation = violation;
    return ViewRef<BoolView>(newOpSetNotEq);
}

std::ostream& OpSetNotEq::dumpState(std::ostream& os) const {
    os << "OpSetNotEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
    return os;
}
