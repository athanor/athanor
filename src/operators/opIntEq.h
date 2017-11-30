
#ifndef SRC_OPERATORS_OPINTEQ_H_
#define SRC_OPERATORS_OPINTEQ_H_
#include "operators/operatorBase.h"
#include "types/bool.h"
struct OpIntEq : public BoolView {
    struct Trigger;
    IntReturning left;
    IntReturning right;
    std::shared_ptr<Trigger> operandTrigger = nullptr;

   public:
    OpIntEq(IntReturning left, IntReturning right)
        : left(std::move(left)), right(std::move(right)) {}

    OpIntEq(const OpIntEq& other) = delete;
    OpIntEq(OpIntEq&& other);
    ~OpIntEq() { stopTriggering(*this); }
};
#endif /* SRC_OPERATORS_OPINTEQ_H_ */
