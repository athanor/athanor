
#ifndef SRC_OPERATORS_OPAND_H_
#define SRC_OPERATORS_OPAND_H_
#include <vector>
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
template <typename T>
class ConjunctionTrigger;
template <typename T>
class ConjunctionIterAssignedTrigger;
struct OpAnd;

typedef ConjunctionTrigger<OpAnd> OpAndTrigger;

struct OpAnd : public BoolView {
    std::vector<BoolReturning> operands;
    FastIterableIntSet violatingOperands;
    std::vector<std::shared_ptr<OpAndTrigger>> operandTriggers;

   public:
    OpAnd(std::vector<BoolReturning> operandsIn)
        : operands(std::move(operandsIn)),
          violatingOperands(0, this->operands.size() - 1) {}
    OpAnd(const OpAnd& other) = delete;
    OpAnd(OpAnd&& other);
    ~OpAnd() { stopTriggering(*this); }

    OpAnd(std::vector<BoolReturning> operands,
          const FastIterableIntSet& violatingOperands)
        : operands(std::move(operands)), violatingOperands(violatingOperands) {}
};
template <typename Op>
class ConjunctionTrigger : public BoolTrigger {
    friend Op;

   protected:
    template <typename Operator, typename Trigger>
    friend void setTriggerParentImpl(Operator* op,
                                     std::shared_ptr<Trigger>& trigger);
    template <typename Operator, typename Trigger>
    friend void setTriggerParentImpl(
        Operator* op, std::vector<std::shared_ptr<Trigger>>& triggers);
    Op* op;
    u_int64_t lastViolation;

   public:
    size_t index;

   public:
    ConjunctionTrigger(Op* op, size_t index) : op(op), index(index) {}
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
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(op->violation); },
            op->triggers);
        op->violation -= lastViolation;
        op->violation += newViolation;
        visitTriggers(
            [&](auto& trigger) { trigger->valueChanged(op->violation); },
            op->triggers);
    }
    void iterHasNewValue(const BoolValue& oldValue,
                         const ValRef<BoolValue>& newValue) final {
        possibleValueChange(oldValue.violation);
        valueChanged(newValue->violation);
    }
};

#endif /* SRC_OPERATORS_OPAND_H_ */
