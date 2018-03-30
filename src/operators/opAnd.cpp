#include "operators/opAnd.h"
#include <algorithm>
#include <cassert>
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OpAnd::OperandsSequenceTrigger;
using OperandTrigger = OpAnd::OperandTrigger;
class OpAnd::OperandTrigger : public BoolTrigger {
   public:
    OpAnd* op;
    UInt lastViolation;
    size_t index;

    OperandTrigger(OpAnd* op, size_t index) : op(op), index(index) {}
    void possibleValueChange(UInt oldVilation) { lastViolation = oldVilation; }
    void valueChanged(UInt newViolation) {
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

class OpAnd::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    OpAnd* op;
    OperandsSequenceTrigger(OpAnd* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
        op->operandTriggers.insert(op->operandTriggers.begin() + index,
                                   std::make_shared<OperandTrigger>(op, index));
        addTrigger(expr, op->operandTriggers[index]);
        for (size_t i = index + 1; i < op->operandTriggers.size(); i++) {
            op->operandTriggers[i]->index = i;
        }
        for (size_t i = op->operandTriggers.size(); i > index + 1; i--) {
            if (op->violatingOperands.erase(i - 1)) {
                op->violatingOperands.insert(i);
            }
        }
        UInt violation = expr->violation;
        if (violation > 0) {
            op->violatingOperands.insert(index);
            op->changeValue([&]() {
                op->violation += violation;
                return true;
            });
        }
    }

    void valueRemoved(UInt index, const AnyExprRef& exprIn) final {
        const auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
        auto trigger = move(op->operandTriggers[index]);
        op->operandTriggers.erase(op->operandTriggers.begin() + index);
        for (size_t i = index; i < op->operandTriggers.size(); i++) {
            op->operandTriggers[i]->index = i;
        }
        UInt violationOfRemovedExpr = expr->violation;
        debug_code(assert((op->violatingOperands.count(index) &&
                           violationOfRemovedExpr > 0) ||
                          (!op->violatingOperands.count(index) &&
                           violationOfRemovedExpr == 0)));
        op->violatingOperands.erase(index);
        for (size_t i = index; i < op->operandTriggers.size(); i++) {
            if (op->violatingOperands.erase(index + 1)) {
                op->violatingOperands.insert(index);
            }
        }
        op->changeValue([&]() {
            op->violation -= violationOfRemovedExpr;
            return true;
        });
    }

    inline void beginSwaps() final {}
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt index1, UInt index2) {
        std::swap(op->operandTriggers[index1], op->operandTriggers[index2]);
        op->operand
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
        if (operand->violation > 0) {
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
      operandSequenceTrigger(move(other.operandSequenceTrigger)) {
    setTriggerParent(this, operandTriggers);
    setTriggerParent(this, operandSequenceTrigger);
}

void OpAnd::startTriggering() {
    if (!operandSequenceTrigger) {
        operandSequenceTrigger =
            std::make_shared<OperandsSequenceTrigger>(this);
        addTrigger(quantifier, operandSequenceTrigger);
        quantifier->startTriggeringOnContainer();

        for (size_t i = 0; i < quantifier->exprs.size(); ++i) {
            auto& operand = quantifier->exprs[i];
            auto trigger = make_shared<OperandTrigger>(this, i);
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
    if (operandSequenceTrigger) {
        deleteTrigger(operandSequenceTrigger);
        operandSequenceTrigger = nullptr;
        quantifier->stopTriggeringOnContainer();
    }
}

void OpAnd::updateViolationDescription(const UInt,
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
    vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
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
