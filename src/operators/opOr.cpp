#include "operators/opOr.h"
#include <algorithm>
#include <cassert>
#include "utils/ignoreUnused.h"
using namespace std;

u_int64_t twoToThe32 = ((u_int64_t)1) << 31;
using QuantifierTrigger = OpOr::QuantifierTrigger;
inline u_int64_t findNewMinViolation(OpOr& op, u_int64_t minViolation) {
    for (size_t i = 0; i < op.quantifier->exprs.size(); ++i) {
        auto& operand = op.quantifier->exprs[i];
        u_int64_t violation = operand->violation;
        if (violation < minViolation) {
            minViolation = violation;
            op.minViolationIndices.clear();
            op.minViolationIndices.insert(i);
        } else if (violation == minViolation) {
            op.minViolationIndices.insert(i);
        }
    }
    return minViolation;
}

class OpOrTrigger : public BoolTrigger {
   public:
    OpOr* op;
    size_t index;

    OpOrTrigger(OpOr* op, size_t index) : op(op), index(index) {}
    void possibleValueChange(u_int64_t) final {}
    void valueChanged(u_int64_t newViolation) final {
        u_int64_t minViolation = op->violation;
        if (newViolation < minViolation) {
            minViolation = newViolation;
            op->minViolationIndices.clear();
            op->minViolationIndices.insert(index);
        } else if (newViolation == minViolation) {
            op->minViolationIndices.insert(index);
        } else if (op->minViolationIndices.size() > 1) {
            op->minViolationIndices.erase(index);
        } else if (op->minViolationIndices.count(index)) {
            // This is the last value holding the current min and it is now been
            // changed.  go through and find the new min
            minViolation = findNewMinViolation(*op, newViolation);
        }
        op->changeValue([&]() {
            op->violation = minViolation;
            return true;
        });
    }

    void iterHasNewValue(const BoolView& oldValue,
                         const ExprRef<BoolView>& newValue) final {
        possibleValueChange(oldValue.violation);
        valueChanged(newValue->violation);
    }
};

class OpOr::QuantifierTrigger
    : public QuantifierView<BoolView>::Trigger {
   public:
    OpOr* op;
    QuantifierTrigger(OpOr* op) : op(op) {}
    void exprUnrolled(const ExprRef<BoolView>& expr) final {
        op->operandTriggers.emplace_back(
            std::make_shared<OpOrTrigger>(op, op->operandTriggers.size()));
        addTrigger(expr, op->operandTriggers.back());
        u_int64_t violation = expr->violation;
        op->operandTriggers.back()->valueChanged(violation);
    }

    void exprRolled(u_int64_t index, const ExprRef<BoolView>&) final {
        op->operandTriggers[index] = std::move(op->operandTriggers.back());
        op->operandTriggers.pop_back();
        op->minViolationIndices.erase(index);
        if (index < op->operandTriggers.size()) {
            op->operandTriggers[index]->index = index;
            if (op->minViolationIndices.erase(op->operandTriggers.size())) {
                op->minViolationIndices.insert(index);
            }
        }
        if (op->minViolationIndices.size() == 0) {
            if (op->operandTriggers.size() == 0) {
                op->changeValue([&]() {
                    op->violation = twoToThe32;
                    return true;
                });
                return;
            } else {
                u_int64_t minViolation = op->quantifier->exprs[0]->violation;
                op->minViolationIndices.insert(0);
                minViolation = findNewMinViolation(*op, minViolation);
                op->changeValue([&]() {
                    op->violation = minViolation;
                    return true;
                });
            }
        }
    }

    void iterHasNewValue(const QuantifierView<BoolView>&,
                         const ExprRef<QuantifierView<BoolView>>&) {
        todoImpl();
    }
};

void OpOr::evaluate() {
    quantifier->initialUnroll();

    if (quantifier->exprs.size() == 0) {
        violation = twoToThe32;
        return;
    }
    for (auto& operand : quantifier->exprs) {
        operand->evaluate();
    }
    u_int64_t minViolation = quantifier->exprs[0]->violation;
    minViolationIndices.insert(0);
    minViolation = findNewMinViolation(*this, minViolation);
    violation = minViolation;
}

OpOr::OpOr(OpOr&& other)
    : BoolView(move(other)),
      quantifier(move(other.quantifier)),
      minViolationIndices(move(other.minViolationIndices)),
      operandTriggers(move(other.operandTriggers)),
      quantifierTrigger(move(other.quantifierTrigger)) {
    setTriggerParent(this, operandTriggers);
    setTriggerParent(this, quantifierTrigger);
}

void OpOr::startTriggering() {
    if (!quantifierTrigger) {
        quantifierTrigger = std::make_shared<QuantifierTrigger>(this);
        addTrigger(quantifier, quantifierTrigger);
        quantifier->startTriggeringOnContainer();

        for (size_t i = 0; i < quantifier->exprs.size(); ++i) {
            auto& operand = quantifier->exprs[i];
            auto trigger = make_shared<OpOrTrigger>(this, i);
            addTrigger(operand, trigger);
            operandTriggers.emplace_back(move(trigger));
            operand->startTriggering();
        }
    }
}

void OpOr::stopTriggering() {
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

void OpOr::updateViolationDescription(u_int64_t,
                                      ViolationDescription& vioDesc) {
    for (auto& operand : quantifier->exprs) {
        operand->updateViolationDescription(violation, vioDesc);
    }
}

ExprRef<BoolView> OpOr::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpOr =
        make_shared<OpOr>(quantifier->deepCopyQuantifierForUnroll(iterator));
    newOpOr->violation = violation;
    newOpOr->minViolationIndices = minViolationIndices;
    return ViewRef<BoolView>(newOpOr);
}

std::ostream& OpOr::dumpState(std::ostream& os) const {
    os << "OpOr: violation=" << violation << endl;
    vector<u_int64_t> sortedMinViolationIndices(minViolationIndices.begin(),
                                                minViolationIndices.end());
    sort(sortedMinViolationIndices.begin(), sortedMinViolationIndices.end());
    os << "Min violating indicies: " << sortedMinViolationIndices << endl;
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
