#include "operators/opIntEq.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;
void evaluate(OpIntEq& op) {
    op.violation = std::abs(getView<IntView>(op.left).value -
                            getView<IntView>(op.right).value);
}

struct OpIntEq::Trigger : public IntTrigger,
                          public IterAssignedTrigger<IntValue> {
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
    op.operandTrigger = make_shared<OpIntEq::Trigger>(&op);
    addTrigger<IntTrigger>(getView<IntView>(op.left).triggers,
                           op.operandTrigger);
    addTrigger<IntTrigger>(getView<IntView>(op.right).triggers,
                           op.operandTrigger);
    startTriggering(op.left);
    startTriggering(op.right);
    auto iterAssignedFunc = overloaded(
        [&](IterRef<IntValue>& ref) {
            addTrigger<IterAssignedTrigger<IntValue>>(
                ref.getIterator().unrollTriggers, op.operandTrigger);
        },
        [](auto&) {});
    mpark::visit(iterAssignedFunc, op.left);
    mpark::visit(iterAssignedFunc, op.right);
}

void stopTriggering(OpIntEq& op) {
    deleteTrigger(op.operandTrigger);
    stopTriggering(op.left);
    stopTriggering(op.right);
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
