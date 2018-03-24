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
    inline void possibleValueChange(Int) final {}
    inline void valueChanged(Int) final {
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

void OpIntEq::updateViolationDescription(UInt,
                                         ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

ExprRef<BoolView> OpIntEq::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpIntEq = make_shared<OpIntEq>(deepCopyForUnroll(left, iterator),
                                           deepCopyForUnroll(right, iterator));
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

ValRef<IntValue> asIntValue(ExprRef<IntView>& expr);
pair<AnyValRef, AnyExprRef> define(ValRef<IntValue>& val,
                                   ExprRef<IntView>& expr);
pair<bool, pair<AnyValRef, AnyExprRef>>
OpIntEq::tryReplaceConstraintWithDefine() {
    auto l = asIntValue(left);
    auto r = asIntValue(right);
    if (l && l->container != &constantPool) {
        return make_pair(true, define(l, right));
    } else if (r && r->container != &constantPool) {
        return make_pair(true, define(r, left));
    }
    return make_pair(false, make_pair(ValRef<BoolValue>(nullptr),
                                      ViewRef<BoolView>(nullptr)));
}

inline ValRef<IntValue> asIntValue(ExprRef<IntView>& expr) {
    if (expr.isViewRef() && dynamic_cast<IntValue*>(&(*expr))) {
        return assumeAsValue(expr.asViewRef());
    } else {
        return ValRef<IntValue>(nullptr);
    }
}

pair<AnyValRef, AnyExprRef> define(ValRef<IntValue>& val,
                                   ExprRef<IntView>& expr) {
    addTrigger(expr, std::make_shared<DefinedTrigger<IntValue>>(val));
    val->container = &constantPool;
    return make_pair(AnyValRef(val), AnyExprRef(expr));
}
