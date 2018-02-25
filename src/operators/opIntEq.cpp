#include "operators/opIntEq.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;
void evaluate(OpIntEq& op) {
    evaluate(op.left);
    evaluate(op.right);
    op.violation = std::abs(getView<IntView>(op.left).value -
                            getView<IntView>(op.right).value);
}

struct OpIntEq::Trigger : public IntTrigger {
    OpIntEq* op;
    Trigger(OpIntEq* op) : op(op) {}
    inline void possibleValueChange(int64_t) final {}
    inline void valueChanged(int64_t) final {
        u_int64_t newViolation = std::abs(getView<IntView>(op->left).value -
                                          getView<IntView>(op->right).value);
        if (newViolation == op->violation) {
            return;
        }
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->violation); },
            op->triggers);
        op->violation = newViolation;
        visitTriggers(
            [&](auto& trigger) { trigger->valueChanged(op->violation); },
            op->triggers);
    }

    inline void iterHasNewValue(const IntValue& oldValue,
                                const ValRef<IntValue>& newValue) final {
        possibleValueChange(oldValue.value);
        valueChanged(newValue->value);
    }
};

OpIntEq::OpIntEq(OpIntEq&& other)
    : BoolView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      operandTrigger(std::move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}

void startTriggering(OpIntEq& op) {
    if (!op.operandTrigger) {
        op.operandTrigger = make_shared<OpIntEq::Trigger>(&op);
        addTrigger(op.left, op.operandTrigger);
        addTrigger(op.right, op.operandTrigger);
        startTriggering(op.left);
        startTriggering(op.right);
    }
}

void stopTriggering(OpIntEq& op) {
    if (op.operandTrigger) {
        deleteTrigger(op.operandTrigger);
        op.operandTrigger = nullptr;
        stopTriggering(op.left);
        stopTriggering(op.right);
    }
}

void updateViolationDescription(const OpIntEq& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.left, op.violation, vioDesc);
    updateViolationDescription(op.right, op.violation, vioDesc);
}

shared_ptr<OpIntEq> deepCopyForUnroll(const OpIntEq& op,
                                      const AnyIterRef& iterator) {
    auto newOpIntEq =
        make_shared<OpIntEq>(deepCopyForUnroll(op.left, iterator),
                             deepCopyForUnroll(op.right, iterator));
    newOpIntEq->violation = op.violation;
    return newOpIntEq;
}

std::ostream& dumpState(std::ostream& os, const OpIntEq& op) {
    os << "OpIntEq: violation=" << op.violation << "\nleft: ";
    dumpState(os, op.left);
    os << "\nright: ";
    dumpState(os, op.right);
    return os;
}
