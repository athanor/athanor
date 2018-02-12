
#ifndef SRC_OPERATORS_OPMOD_H_
#define SRC_OPERATORS_OPMOD_H_
#include "operators/operatorBase.h"
#include "types/int.h"

struct OpMod : public IntView {
    struct Trigger;
    IntReturning left;
    IntReturning right;
    std::shared_ptr<Trigger> operandTrigger = nullptr;

   public:
    OpMod(IntReturning left, IntReturning right)
        : left(std::move(left)), right(std::move(right)) {}

    OpMod(const OpMod& other) = delete;
    OpMod(OpMod&& other);
    ~OpMod() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPMOD_H_ */
