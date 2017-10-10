#ifndef SRC_OPERATORS_BOOLOPERATORS_H_
#define SRC_OPERATORS_BOOLOPERATORS_H_
#include <vector>
#include "operators/boolProducing.h"

struct OpAnd {
    u_int64_t violation = 0;
    std::vector<BoolProducing> operands;
    std::vector<std::shared_ptr<BoolTrigger>> triggers;

   private:
    class Trigger : public BoolTrigger {
        OpAnd& op;
        u_int64_t lastViolation;

       public:
        Trigger(OpAnd& op) : op(op) {}
        void possibleValueChange(u_int64_t oldVilation) {
            lastViolation = oldVilation;
        }
        void valueChanged(u_int64_t newViolation) {
            if (newViolation == lastViolation) {
                return;
            }
            for (auto& trigger : op.triggers) {
                trigger->possibleValueChange(op.violation);
            }
            op.violation -= lastViolation;
            op.violation += newViolation;
            for (auto& trigger : op.triggers) {
                trigger->valueChanged(op.violation);
            }
        }
    };

   public:
    OpAnd(std::vector<BoolProducing> operandsIn)
        : operands(std::move(operandsIn)) {
        auto trigger = std::make_shared<Trigger>(*this);
        for (auto& operand : operands) {
            getBoolView(operand).triggers.emplace_back(trigger);
        }
    }
};

#endif /* SRC_OPERATORS_BOOLOPERATORS_H_ */
