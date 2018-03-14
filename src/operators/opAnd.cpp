#include "operators/opAnd.h"
#include <algorithm>
#include <cassert>
#include "utils/ignoreUnused.h"
using namespace std;
using QuantifierTrigger = OpAnd::QuantifierTrigger;
class OpAndTrigger : public BoolTrigger {
   public:
    OpAnd* op;
    u_int64_t lastViolation;
    size_t index;

    OpAndTrigger(OpAnd* op, size_t index) : op(op), index(index) {}
    void possibleValueChange(u_int64_t oldVilation) {
        lastViolation = oldVilation;
    }
    void valueChanged(u_int64_t newViolation) {
        if (newViolation == lastViolation) {
            return;
        }
        if (newViolation > 0 && lastViolation == 0) {
            op->violatingOperands.insert(index);
        } else if (newViolation == 0 && lastViolation > 0) {
            op->violatingOperands.erase(index);
        }
        op->changeValue([&]() {
            op->violation -= lastViolation;
            op->violation += newViolation;
            return true;
        });
    }

    void iterHasNewValue(const BoolView& oldValue,
                         const ExprRef<BoolView>& newValue) final {
        possibleValueChange(oldValue.violation);
        valueChanged(newValue->violation);
    }
};

class OpAnd::QuantifierTrigger
    : public QuantifierView<BoolView>::Trigger {
   public:
    OpAnd* op;
    QuantifierTrigger(OpAnd* op) : op(op) {}
    void exprUnrolled(const ExprRef<BoolView>& expr) final {
        op->operandTriggers.emplace_back(
            std::make_shared<OpAndTrigger>(op, op->operandTriggers.size()));
        addTrigger(expr, op->operandTriggers.back());
        u_int64_t violation = expr->violation;
        if (violation > 0) {
            op->violatingOperands.insert(op->operandTriggers.size() - 1);
            op->changeValue([&]() {
                op->violation += violation;
                return true;
            });
        }
    }

    void exprRolled(u_int64_t index, const ExprRef<BoolView>& expr) final {
        op->operandTriggers[index] = std::move(op->operandTriggers.back());
        op->operandTriggers.pop_back();
        if (index < op->operandTriggers.size()) {
            op->operandTriggers[index]->index = index;
        }
        u_int64_t violationOfRemovedExpr = expr->violation;
        debug_code(assert((op->violatingOperands.count(index) &&
                           violationOfRemovedExpr > 0) ||
                          (!op->violatingOperands.count(index) &&
                           violationOfRemovedExpr == 0)));
        op->violatingOperands.erase(index);
        if (index < op->operandTriggers.size() &&
            op->violatingOperands.erase(op->operandTriggers.size())) {
            op->violatingOperands.insert(index);
        }
        op->changeValue([&]() {
            op->violation -= violationOfRemovedExpr;
            return true;
        });
    }

    void iterHasNewValue(const QuantifierView<BoolView>&,
                         const ExprRef<QuantifierView<BoolView>>&) {
        todoImpl();
    }
};

void OpAnd::evaluate() {
    quantifier->initialUnroll();
    violation = 0;
    for (size_t i = 0; i < quantifier->exprs.size(); ++i) {
        auto& operand = quantifier->exprs[i];
        operand->evaluate();
        u_int64_t violation = operand->violation;
        if (violation > 0) {
            violatingOperands.insert(i);
        }
        violation += operand->violation;
    }
}

OpAnd::OpAnd(OpAnd&& other)
    : BoolView(move(other)),
      quantifier(move(other.quantifier)),
      violatingOperands(move(other.violatingOperands)),
      operandTriggers(move(other.operandTriggers)),
      quantifierTrigger(move(other.quantifierTrigger)) {
    setTriggerParent(this, operandTriggers);
    setTriggerParent(this, quantifierTrigger);
}

void OpAnd::startTriggering() {
    if (!quantifierTrigger) {
        quantifierTrigger = std::make_shared<QuantifierTrigger>(this);
        addTrigger(quantifier, quantifierTrigger);
        quantifier->startTriggeringOnContainer();

        for (size_t i = 0; i < quantifier->exprs.size(); ++i) {
            auto& operand = quantifier->exprs[i];
            auto trigger = make_shared<OpAndTrigger>(this, i);
            addTrigger(operand, trigger);
            operandTriggers.emplace_back(move(trigger));
            operand->startTriggering();
        }
    }
}

void OpAnd::stopTriggering() {
    while (!operandTriggers.empty()) {
        deleteTrigger(operandTriggers.back());
        operandTriggers.pop_back();
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

void OpAnd::updateViolationDescription(const u_int64_t,
                                       ViolationDescription& vioDesc) {
    for (size_t violatingOperandIndex : violatingOperands) {
        quantifier->exprs[violatingOperandIndex]->updateViolationDescription(
            violation, vioDesc);
    }
}

ExprRef<BoolView> OpAnd::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpAnd =
        make_shared<OpAnd>(quantifier->deepCopyQuantifierForUnroll(iterator));
    newOpAnd->violation = violation;
    newOpAnd->violatingOperands = violatingOperands;
    return ViewRef<BoolView>(newOpAnd);
}

std::ostream& OpAnd::dumpState(std::ostream& os) const {
    os << "OpAnd: violation=" << violation << endl;
    vector<u_int64_t> sortedViolatingOperands(violatingOperands.begin(),
                                              violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
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
