#include "operators/opAnd.h"
#include <cassert>
#include "utils/ignoreUnused.h"
using namespace std;

class OpAndTrigger : public BoolTrigger {
   public:
    OpAnd* op;
    u_int64_t lastViolation;
    size_t index;

    OpAndTrigger(Op* op, size_t index) : op(op), index(index) {}
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

    void iterHasNewValue(const BoolValue& oldValue,
                         const ValRef<BoolValue>& newValue) final {
        possibleValueChange(oldValue.violation);
        valueChanged(newValue->violation);
    }
};

class QuantifierTrigger : public QuantifierView<BoolReturning>::Trigger {
    OpAnd* op;
    inline void exprUnrolled(const BoolReturning& expr) {
        op->operandTriggers.emplace_back(
            std::make_shared<OpAndTrigger>(op, op->operandTriggers.size()));
        addTrigger(expr, op->operandTriggers.back());
        u_int64_t violation = getView<BoolView>(expr).violation;
        if (violation > 0) {
            op->violatingOperands.insert(op->operandTriggers.size() - 1);
            op->changeValue([&]() {
                op->violation += violation;
                return true;
            });
        }
    }

    inline void exprRolled(u_int64_t index, const BoolReturning& expr) final {
        op->operandTriggers[index] = std::move(op->operandTriggers.back());
        op->operandTriggers.pop_back();
        if (index < op->operands.size()) {
            op->operandTriggers[index].index = index;
        }
        u_int64_t violationOfRemovedExpr = getView<BoolView>(expr).violation;
        debug_code(assert((op->violatingOperands.count(index) &&
                           violationOfRemovedExpr > 0) ||
                          (!op->violatingOperands.count(index) &&
                           violationOfRemovedExpr == 0)));
        // if the expr/trigger moved from the end into index had a violation but
        // the expr previously at at index did not, then add the violation to
        // the index which now holds the moved expr
        if (op->violatingOperands.erase(op->operandTriggers.size()) &&
            !op->violatingOperands.count(index)) {
            op->violatingOperands.insert(index);
            // otherwise if the expr previously at index had a violation, it is
            // now gone
        } else if (op->violatingOperands.count(index)) {
            op->violatingOperands.erase(index);
        }
        op->changeValue([&]() { op->violation -= violationOfRemovedExpr; });
    }
};

void evaluate(OpAnd& op) {
    op.quantifier->initialUnroll();
    op.violation = 0;
    for (size_t i = 0; i < op.quantifier.exprs.size(); ++i) {
        auto& operand = op.quantifier.exprs[i];
        evaluate(operand);
        u_int64_t violation = getView<BoolView>(operand).violation;
        if (violation > 0) {
            op.violatingOperands.insert(i);
        }
        op.violation += getView<BoolView>(operand).violation;
    }
}

OpAnd::OpAnd(OpAnd&& other)
    : BoolView(move(other)),
      quantifier.exprs(move(other.quantifier.exprs)),
      violatingOperands(move(other.violatingOperands)),
      operandTriggers(move(other.operandTriggers)),
      quantifierTrigger(move(other.quantifierTrigger)) {
    setTriggerParent(this, operandTriggers);
    setTriggerParent(this, quantifierTrigger);
}

void startTriggering(OpAnd& op) {
    op.quantifierTrigger = std::make_shared<QuantifierTrigger>(&op);
    addTrigger(op.quantifier, op.quantifierTrigger);
    op.quantifier->startTriggeringOnContainer();

    for (size_t i = 0; i < op.quantifier.exprs.size(); ++i) {
        auto& operand = op.quantifier.exprs[i];
        auto trigger = make_shared<OpAndTrigger>(&op, i);
        addTrigger(operand, trigger);
        op.operandTriggers.emplace_back(move(trigger));
        startTriggering(operand);
    }
}

void stopTriggering(OpAnd& op) {
    while (!op.operandTriggers.empty()) {
        deleteTrigger(op.operandTriggers.back());
        op.operandTriggers.pop_back();
    }
    for (auto& operand : op.quantifier.exprs) {
        stopTriggering(operand);
    }
    if (op.quantifierTrigger) {
        deleteTrigger(op.quantifierTrigger);
        op.quantifierTrigger = nullptr;
        op.quantifier->stopTriggeringOnContainer();
    }
}

void updateViolationDescription(const OpAnd& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    for (size_t violatingOperandIndex : op.violatingOperands) {
        updateViolationDescription(op.quantifier.exprs[violatingOperandIndex],
                                   op.violation, vioDesc);
    }
}

shared_ptr<OpAnd> deepCopyForUnroll(const OpAnd& op,
                                    const AnyIterRef& iterator) {
    auto newOpAnd = make_shared<OpAnd>(op->deepCopyQuantifierForUnroll(),
                                       op.violatingOperands);
    newOpAnd->violation = op.violation;
    newOpAndAnd->violatingOperands = op.violatingOperands;
    return newOpAnd;
}

std::ostream& dumpState(std::ostream& os, const OpAnd& op) {
    os << "OpAnd: violation=" << op.violation << "\noperands [";
    bool first = true;
    for (auto& operand : op.quantifier.exprs) {
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
