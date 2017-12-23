#include "operators/opProd.h"
#include <cassert>
using namespace std;
void evaluate(OpProd& op) {
    op.value = 1;
    for (auto& operand : op.operands) {
        evaluate(operand);
        op.value *= getView<IntView>(operand).value;
    }
}

class OpProdTrigger : public IntTrigger {
   public:
    friend OpProd;
    OpProd* op;
    int64_t lastMemberValue;

   public:
    OpProdTrigger(OpProd* op) : op(op) {}
    void possibleValueChange(int64_t oldValue) { lastMemberValue = oldValue; }
    void valueChanged(int64_t newValue) {
        if (newValue == lastMemberValue) {
            return;
        }
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->value); },
            op->triggers);
        op->value /= lastMemberValue;
        op->value *= newValue;
        visitTriggers([&](auto& trigger) { trigger->valueChanged(op->value); },
                      op->triggers);
    }
    inline void iterHasNewValue(const IntValue& oldValue,
                                const ValRef<IntValue>& newValue) {
        possibleValueChange(oldValue.value);
        valueChanged(newValue->value);
    }
};

OpProd::OpProd(OpProd&& other)
    : IntView(move(other)),
      operands(move(other.operands)),
      operandTrigger(move(other.operandTrigger)) {
    setTriggerParent(this, operandTrigger);
}

void startTriggering(OpProd& op) {
    op.operandTrigger = make_shared<OpProdTrigger>(&op);
    for (auto& operand : op.operands) {
        addTrigger(operand, op.operandTrigger);
        startTriggering(operand);
    }
}

void stopTriggering(OpProd& op) {
    if (op.operandTrigger) {
        deleteTrigger(op.operandTrigger);
        op.operandTrigger = nullptr;
        for (auto& operand : op.operands) {
            stopTriggering(operand);
        }
    }
}

void updateViolationDescription(const OpProd& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    for (auto& operand : op.operands) {
        updateViolationDescription(operand, parentViolation, vioDesc);
    }
}

shared_ptr<OpProd> deepCopyForUnroll(const OpProd& op,
                                     const AnyIterRef& iterator) {
    vector<IntReturning> operands;
    operands.reserve(op.operands.size());
    for (auto& operand : op.operands) {
        operands.emplace_back(deepCopyForUnroll(operand, iterator));
    }
    auto newOpProd = make_shared<OpProd>(move(operands));
    newOpProd->value = op.value;
    return newOpProd;
}

std::ostream& dumpState(std::ostream& os, const OpProd& op) {
    os << "OpProd: value=" << op.value << "\noperands [";
    bool first = true;
    for (auto& operand : op.operands) {
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
