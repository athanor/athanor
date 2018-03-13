#include "operators/opIntEq.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpIntEq::evaluate() {
    left->evaluate();
    right->evaluate();
    violation = std::abs(left->value - right->value);
}

struct OpIntEq::Trigger : public IntTrigger {
    OpIntEq* op;
    Trigger(OpIntEq* op) : op(op) {}
    inline void possibleValueChange(int64_t) final {}
    inline void valueChanged(int64_t) final {
        op->changeValue([&]() {
            op->violation = std::abs(op->left->value - op->right->value);
            return true;
        });
    }

    inline void iterHasNewValue(const IntView& oldValue,
                                const ExprRef<IntView>& newValue) final {
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

void OpIntEq::startTriggering() {
    if (!operandTrigger) {
        operandTrigger = make_shared<OpIntEq::Trigger>(this);
        addTrigger(left, operandTrigger);
        addTrigger(right, operandTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void OpIntEq::stopTriggering() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
        left->stopTriggering();
        right->stopTriggering();
    }
}

void OpIntEq::updateViolationDescription(u_int64_t,
                                         ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

ExprRef<BoolView> OpIntEq::deepCopyForUnroll(const ExprRef<BoolView>&,
                                             const AnyIterRef& iterator) const {
    auto newOpIntEq =
        make_shared<OpIntEq>(left->deepCopyForUnroll(left, iterator),
                             right->deepCopyForUnroll(right, iterator));
    newOpIntEq->violation = violation;
    return ViewRef<BoolView>(newOpIntEq);
}

std::ostream& OpIntEq::dumpState(std::ostream& os) const {
    os << "OpIntEq: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}
