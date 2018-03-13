#include "operators/opProd.h"
#include <cassert>
#include "utils/ignoreUnused.h"

using namespace std;
using QuantifierTrigger = OpProd::QuantifierTrigger;
class OpProdTrigger : public IntTrigger {
   public:
    OpProd* op;
    int64_t lastValue;

    OpProdTrigger(OpProd* op) : op(op) {}
    void possibleValueChange(int64_t oldValue) { lastValue = oldValue; }
    void valueChanged(int64_t newValue) {
        op->changeValue([&]() {
            op->value /= lastValue;
            op->value *= newValue;
            return true;
        });
    }

    void iterHasNewValue(const IntView& oldValue,
                         const ExprRef<IntView>& newValue) final {
        possibleValueChange(oldValue.value);
        valueChanged(newValue->value);
    }
};

class OpProd::QuantifierTrigger
    : public QuantifierView<ExprRef<IntView>>::Trigger {
   public:
    OpProd* op;
    QuantifierTrigger(OpProd* op) : op(op) {}
    void exprUnrolled(const ExprRef<IntView>& expr) final {
        u_int64_t value = expr->value;
        addTrigger(expr, op->operandTrigger);
        op->changeValue([&]() {
            op->value *= value;
            return true;
        });
    }

    void exprRolled(u_int64_t, const ExprRef<IntView>& expr) final {
        int64_t valueOfRemovedExpr = expr->value;
        op->changeValue([&]() {
            op->value /= valueOfRemovedExpr;
            return true;
        });
    }

    void iterHasNewValue(const QuantifierView<ExprRef<IntView>>&,
                         const ExprRef<QuantifierView<ExprRef<IntView>>>&) {
        todoImpl();
    }
};

void OpProd::evaluate() {
    quantifier->initialUnroll();
    value = 1;
    for (size_t i = 0; i < quantifier->exprs.size(); ++i) {
        auto& operand = quantifier->exprs[i];
        operand->evaluate();
        value *= operand->value;
    }
}

OpProd::OpProd(OpProd&& other)
    : IntView(move(other)),
      quantifier(move(other.quantifier)),
      operandTrigger(move(other.operandTrigger)),
      quantifierTrigger(move(other.quantifierTrigger)) {
    setTriggerParent(this, operandTrigger, quantifierTrigger);
}

void OpProd::startTriggering() {
    if (!quantifierTrigger) {
        quantifierTrigger = std::make_shared<QuantifierTrigger>(this);
        addTrigger(quantifier, quantifierTrigger);
        quantifier->startTriggeringOnContainer();
        operandTrigger = make_shared<OpProdTrigger>(this);
        for (size_t i = 0; i < quantifier->exprs.size(); ++i) {
            auto& operand = quantifier->exprs[i];
            addTrigger(operand, operandTrigger);
            operand->startTriggering();
        }
    }
}

void OpProd::stopTriggering() {
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

void OpProd::updateViolationDescription(u_int64_t parentViolation,
                                        ViolationDescription& vioDesc) {
    if (!quantifier) {
        return;
    }
    for (auto& operand : quantifier->exprs) {
        operand->updateViolationDescription(parentViolation, vioDesc);
    }
}

ExprRef<IntView> OpProd::deepCopyForUnroll(const ExprRef<IntView>&,
                                           const AnyIterRef& iterator) const {
    auto newOpProd =
        make_shared<OpProd>(quantifier->deepCopyQuantifierForUnroll(iterator));
    newOpProd->value = value;
    return ViewRef<IntView>(newOpProd);
}

std::ostream& OpProd::dumpState(std::ostream& os) const {
    os << "OpProd: value=" << value << endl;
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
