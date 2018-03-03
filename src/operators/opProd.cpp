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

    void iterHasNewValue(const IntValue& oldValue,
                         const ValRef<IntValue>& newValue) final {
        possibleValueChange(oldValue.value);
        valueChanged(newValue->value);
    }
};

class OpProd::QuantifierTrigger : public QuantifierView<IntReturning>::Trigger {
   public:
    OpProd* op;
    QuantifierTrigger(OpProd* op) : op(op) {}
    void exprUnrolled(const IntReturning& expr) final {
        u_int64_t value = getView(expr).value;
        addTrigger(expr, op->operandTrigger);
        op->changeValue([&]() {
            op->value *= value;
            return true;
        });
    }

    void exprRolled(u_int64_t, const IntReturning& expr) final {
        int64_t valueOfRemovedExpr = getView(expr).value;
        op->changeValue([&]() {
            op->value /= valueOfRemovedExpr;
            return true;
        });
    }

    void iterHasNewValue(const QuantifierView<IntReturning>&,
                         const ValRef<QuantifierView<IntReturning>>&) {
        todoImpl();
    }
};

void evaluate(OpProd& op) {
    op.quantifier->initialUnroll();
    op.value = 1;
    for (size_t i = 0; i < op.quantifier->exprs.size(); ++i) {
        auto& operand = op.quantifier->exprs[i];
        evaluate(operand);
        op.value *= getView(operand).value;
    }
}

OpProd::OpProd(OpProd&& other)
    : IntView(move(other)),
      quantifier(move(other.quantifier)),
      operandTrigger(move(other.operandTrigger)),
      quantifierTrigger(move(other.quantifierTrigger)) {
    setTriggerParent(this, operandTrigger, quantifierTrigger);
}

void startTriggering(OpProd& op) {
    if (!op.quantifierTrigger) {
        op.quantifierTrigger = std::make_shared<QuantifierTrigger>(&op);
        addTrigger(op.quantifier, op.quantifierTrigger);
        op.quantifier->startTriggeringOnContainer();
        op.operandTrigger = make_shared<OpProdTrigger>(&op);
        for (size_t i = 0; i < op.quantifier->exprs.size(); ++i) {
            auto& operand = op.quantifier->exprs[i];
            addTrigger(operand, op.operandTrigger);
            startTriggering(operand);
        }
    }
}

void stopTriggering(OpProd& op) {
    if (op.operandTrigger) {
        deleteTrigger(op.operandTrigger);
        op.operandTrigger = nullptr;
    }
    if (op.quantifier) {
        for (auto& operand : op.quantifier->exprs) {
            stopTriggering(operand);
        }
    }
    if (op.quantifierTrigger) {
        deleteTrigger(op.quantifierTrigger);
        op.quantifierTrigger = nullptr;
        op.quantifier->stopTriggeringOnContainer();
    }
}

void updateViolationDescription(const OpProd& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    if (!op.quantifier) {
        return;
    }
    for (auto& operand : op.quantifier->exprs) {
        updateViolationDescription(operand, parentViolation, vioDesc);
    }
}

shared_ptr<OpProd> deepCopyForUnroll(const OpProd& op,
                                     const AnyIterRef& iterator) {
    auto newOpProd = make_shared<OpProd>(
        op.quantifier->deepCopyQuantifierForUnroll(iterator));
    newOpProd->value = op.value;
    return newOpProd;
}

std::ostream& dumpState(std::ostream& os, const OpProd& op) {
    os << "OpProd: value=" << op.value << endl;
    os << "operands [";
    bool first = true;
    for (auto& operand : op.quantifier->exprs) {
        if (first) {
            first = false;
        } else {
            os << ",\n";
        }
        dumpState(os, operand);
    }
    os << "]";
    return os;
}
