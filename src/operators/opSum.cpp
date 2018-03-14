#include "operators/opSum.h"
#include <cassert>
#include "utils/ignoreUnused.h"

using namespace std;
using QuantifierTrigger = OpSum::QuantifierTrigger;
class OpSumTrigger : public IntTrigger {
   public:
    OpSum* op;
    int64_t lastValue;

    OpSumTrigger(OpSum* op) : op(op) {}
    void possibleValueChange(int64_t oldValue) { lastValue = oldValue; }
    void valueChanged(int64_t newValue) {
        op->changeValue([&]() {
            op->value -= lastValue;
            op->value += newValue;
            return true;
        });
    }

    void iterHasNewValue(const IntView& oldValue,
                         const ExprRef<IntView>& newValue) final {
        possibleValueChange(oldValue.value);
        valueChanged(newValue->value);
    }
};

class OpSum::QuantifierTrigger
    : public QuantifierView<ExprRef<IntView>>::Trigger {
   public:
    OpSum* op;
    QuantifierTrigger(OpSum* op) : op(op) {}
    void exprUnrolled(const ExprRef<IntView>& expr) final {
        u_int64_t value = expr->value;
        addTrigger(expr, op->operandTrigger);
        op->changeValue([&]() {
            op->value += value;
            return true;
        });
    }

    void exprRolled(u_int64_t, const ExprRef<IntView>& expr) final {
        int64_t valueOfRemovedExpr = expr->value;
        op->changeValue([&]() {
            op->value -= valueOfRemovedExpr;
            return true;
        });
    }

    void iterHasNewValue(const QuantifierView<ExprRef<IntView>>&,
                         const ExprRef<QuantifierView<ExprRef<IntView>>>&) {
        todoImpl();
    }
};

void OpSum::evaluate() {
    quantifier->initialUnroll();
    value = 0;
    for (size_t i = 0; i < quantifier->exprs.size(); ++i) {
        auto& operand = quantifier->exprs[i];
        operand->evaluate();
        value += operand->value;
    }
}

OpSum::OpSum(OpSum&& other)
    : IntView(move(other)),
      quantifier(move(other.quantifier)),
      operandTrigger(move(other.operandTrigger)),
      quantifierTrigger(move(other.quantifierTrigger)) {
    setTriggerParent(this, operandTrigger, quantifierTrigger);
}

void OpSum::startTriggering() {
    if (!quantifierTrigger) {
        quantifierTrigger = std::make_shared<QuantifierTrigger>(this);
        addTrigger(quantifier, quantifierTrigger);
        quantifier->startTriggeringOnContainer();
        operandTrigger = make_shared<OpSumTrigger>(this);
        for (size_t i = 0; i < quantifier->exprs.size(); ++i) {
            auto& operand = quantifier->exprs[i];
            addTrigger(operand, operandTrigger);
            operand->startTriggering();
        }
    }
}

void OpSum::stopTriggering() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
    }
    if (quantifier) {
        for (auto& operand : quantifier->exprs) {
            operand->stopTriggering();
        }
    }
    if (quantifierTrigger) {
        deleteTrigger(quantifierTrigger);
        quantifierTrigger = nullptr;
        quantifier->stopTriggeringOnContainer();
    }
}

void OpSum::updateViolationDescription(u_int64_t parentViolation,
                                       ViolationDescription& vioDesc) {
    if (!quantifier) {
        return;
    }
    for (auto& operand : quantifier->exprs) {
        operand->updateViolationDescription(parentViolation, vioDesc);
    }
}

ExprRef<IntView> OpSum::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpSum =
        make_shared<OpSum>(quantifier->deepCopyQuantifierForUnroll(iterator));
    newOpSum->value = value;
    return ViewRef<IntView>(newOpSum);
}

std::ostream& OpSum::dumpState(std::ostream& os) const {
    os << "OpSum: value=" << value << endl;
    os << "operands [";
    bool first = true;
    for (auto& operand : quantifier->exprs) {
        if (first) {
            first = false;
        } else {
            os << ",\n";
        }
        operand->dumpState(os);
    }
    os << "]";
    return os;
}
