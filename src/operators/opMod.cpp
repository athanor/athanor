#include "operators/opMod.h"
#include <cassert>
#include <cstdlib>
#include "types/int.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpMod::evaluate() {
    left->evaluate();
    right->evaluate();
    value = left->value % right->value;
}

struct OpMod::Trigger : public IntTrigger {
    OpMod* op;
    Trigger(OpMod* op) : op(op) {}
    inline void possibleValueChange(int64_t) final {}
    inline void valueChanged(int64_t) final {
        op->changeValue([&]() {
            op->value = op->left->value % op->right->value;
            return true;
        });
    }

    inline void iterHasNewValue(const IntView& oldValue,
                                const ExprRef<IntView>& newValue) final {
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

void OpMod::startTriggering() {
    if (!operandTrigger) {
        operandTrigger = make_shared<OpMod::Trigger>(this);
        addTrigger(left, operandTrigger);
        addTrigger(right, operandTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void OpMod::stopTriggering() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
        left->stopTriggering();
        right->stopTriggering();
    }
}

void OpMod::updateViolationDescription(u_int64_t parentViolation,
                                       ViolationDescription& vioDesc) {
    left->updateViolationDescription(parentViolation, vioDesc);
    right->updateViolationDescription(parentViolation, vioDesc);
}

<<<<<<< Updated upstream
ExprRef<IntView> OpMod::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpMod = make_shared<OpMod>(deepCopyForUnroll(left, iterator),
                                       deepCopyForUnroll(right, iterator));
=======
ExprRef<IntView> OpMod::deepCopySelfForUnroll(const ExprRef<IntView>&,
                                          const AnyIterRef& iterator) const {
    auto newOpMod =
        make_shared<OpMod>(lefdeepCopyForUnroll(left, iterator),
                           righdeepCopyForUnroll(right, iterator));
>>>>>>> Stashed changes
    newOpMod->value = value;
    return ViewRef<IntView>(newOpMod);
}

std::ostream& OpMod::dumpState(std::ostream& os) const {
    os << "OpMod: value=" << value << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}
