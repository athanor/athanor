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

    void iterHasNewValue(const BoolValue& oldValue,
                         const ValRef<BoolValue>& newValue) final {
        possibleValueChange(oldValue.violation);
        valueChanged(newValue->violation);
    }
};

class OpAnd::QuantifierTrigger : public QuantifierView<BoolReturning>::Trigger {
   public:
    OpAnd* op;
    QuantifierTrigger(OpAnd* op) : op(op) {}
    void exprUnrolled(const BoolReturning& expr) final {
        op->operandTriggers.emplace_back(
            std::make_shared<OpAndTrigger>(op, op->operandTriggers.size()));
        addTrigger(expr, op->operandTriggers.back());
        u_int64_t violation = getView(expr).violation;
        if (violation > 0) {
            op->violatingOperands.insert(op->operandTriggers.size() - 1);
            op->changeValue([&]() {
                op->violation += violation;
                return true;
            });
        }
    }

    void exprRolled(u_int64_t index, const BoolReturning& expr) final {
        op->operandTriggers[index] = std::move(op->operandTriggers.back());
        op->operandTriggers.pop_back();
        if (index < op->operandTriggers.size()) {
            op->operandTriggers[index]->index = index;
        }
        u_int64_t violationOfRemovedExpr = getView(expr).violation;
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

    void iterHasNewValue(const QuantifierView<BoolReturning>&,
                         const ValRef<QuantifierView<BoolReturning>>&) {
        assert(false);  // todo
    }
};

void evaluate(OpAnd& op) {
    op.quantifier->initialUnroll();
    op.violation = 0;
    for (size_t i = 0; i < op.quantifier->exprs.size(); ++i) {
        auto& operand = op.quantifier->exprs[i];
        evaluate(operand);
        u_int64_t violation = getView(operand).violation;
        if (violation > 0) {
            op.violatingOperands.insert(i);
        }
        op.violation += getView(operand).violation;
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

void startTriggering(OpAnd& op) {
    if (!op.quantifierTrigger) {
        op.quantifierTrigger = std::make_shared<QuantifierTrigger>(&op);
        addTrigger(op.quantifier, op.quantifierTrigger);
        op.quantifier->startTriggeringOnContainer();

        for (size_t i = 0; i < op.quantifier->exprs.size(); ++i) {
            auto& operand = op.quantifier->exprs[i];
            auto trigger = make_shared<OpAndTrigger>(&op, i);
            addTrigger(operand, trigger);
            op.operandTriggers.emplace_back(move(trigger));
            startTriggering(operand);
        }
    }
}

void stopTriggering(OpAnd& op) {
    while (!op.operandTriggers.empty()) {
        deleteTrigger(op.operandTriggers.back());
        op.operandTriggers.pop_back();
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

void updateViolationDescription(const OpAnd& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    for (size_t violatingOperandIndex : op.violatingOperands) {
        updateViolationDescription(op.quantifier->exprs[violatingOperandIndex],
                                   op.violation, vioDesc);
    }
}

shared_ptr<OpAnd> deepCopyForUnroll(const OpAnd& op,
                                    const AnyIterRef& iterator) {
    auto newOpAnd = make_shared<OpAnd>(
        op.quantifier->deepCopyQuantifierForUnroll(iterator));
    newOpAnd->violation = op.violation;
    newOpAnd->violatingOperands = op.violatingOperands;
    return newOpAnd;
}

std::ostream& dumpState(std::ostream& os, const OpAnd& op) {
    os << "OpAnd: violation=" << op.violation << endl;
    vector<u_int64_t> sortedViolatingOperands(op.violatingOperands.begin(),
                                              op.violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
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
