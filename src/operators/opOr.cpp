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
        u_int64_t violation = getView(operand).violation;
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

    void iterHasNewValue(const BoolValue& oldValue,
                         const ValRef<BoolValue>& newValue) final {
        possibleValueChange(oldValue.violation);
        valueChanged(newValue->violation);
    }
};

class OpOr::QuantifierTrigger : public QuantifierView<BoolReturning>::Trigger {
   public:
    OpOr* op;
    QuantifierTrigger(OpOr* op) : op(op) {}
    void exprUnrolled(const BoolReturning& expr) final {
        op->operandTriggers.emplace_back(
            std::make_shared<OpOrTrigger>(op, op->operandTriggers.size()));
        addTrigger(expr, op->operandTriggers.back());
        u_int64_t violation = getView(expr).violation;
        op->operandTriggers.back()->valueChanged(violation);
    }

    void exprRolled(u_int64_t index, const BoolReturning&) final {
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
                u_int64_t minViolation =
                    getView(op->quantifier->exprs[0]).violation;
                op->minViolationIndices.insert(0);
                minViolation = findNewMinViolation(*op, minViolation);
                op->changeValue([&]() {
                    op->violation = minViolation;
                    return true;
                });
            }
        }
    }

    void iterHasNewValue(const QuantifierView<BoolReturning>&,
                         const ValRef<QuantifierView<BoolReturning>>&) {
        todoImpl();
    }
};

void evaluate(OpOr& op) {
    op.quantifier->initialUnroll();

    if (op.quantifier->exprs.size() == 0) {
        op.violation = twoToThe32;
        return;
    }
    for (auto& operand : op.quantifier->exprs) {
        evaluate(operand);
    }
    u_int64_t minViolation = getView(op.quantifier->exprs[0]).violation;
    op.minViolationIndices.insert(0);
    minViolation = findNewMinViolation(op, minViolation);
    op.violation = minViolation;
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

void startTriggering(OpOr& op) {
    if (!op.quantifierTrigger) {
        op.quantifierTrigger = std::make_shared<QuantifierTrigger>(&op);
        addTrigger(op.quantifier, op.quantifierTrigger);
        op.quantifier->startTriggeringOnContainer();

        for (size_t i = 0; i < op.quantifier->exprs.size(); ++i) {
            auto& operand = op.quantifier->exprs[i];
            auto trigger = make_shared<OpOrTrigger>(&op, i);
            addTrigger(operand, trigger);
            op.operandTriggers.emplace_back(move(trigger));
            startTriggering(operand);
        }
    }
}

void stopTriggering(OpOr& op) {
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

void updateViolationDescription(const OpOr& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    for (auto& operand : op.quantifier->exprs) {
        updateViolationDescription(operand, op.violation, vioDesc);
    }
}

shared_ptr<OpOr> deepCopyForUnroll(const OpOr& op, const AnyIterRef& iterator) {
    auto newOpOr =
        make_shared<OpOr>(op.quantifier->deepCopyQuantifierForUnroll(iterator));
    newOpOr->violation = op.violation;
    newOpOr->minViolationIndices = op.minViolationIndices;
    return newOpOr;
}

std::ostream& dumpState(std::ostream& os, const OpOr& op) {
    os << "OpOr: violation=" << op.violation << endl;
    vector<u_int64_t> sortedMinViolationIndices(op.minViolationIndices.begin(),
                                                op.minViolationIndices.end());
    sort(sortedMinViolationIndices.begin(), sortedMinViolationIndices.end());
    os << "Min violating indicies: " << sortedMinViolationIndices << endl;
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
