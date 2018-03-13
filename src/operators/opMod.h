
#ifndef SRC_OPERATORS_OPMOD_H_
#define SRC_OPERATORS_OPMOD_H_
#include "operators/operatorBase.h"
#include "types/int.h"

struct OpMod : public IntView {
    struct Trigger;
    ExprRef<IntView> left;
    ExprRef<IntView> right;
    std::shared_ptr<Trigger> operandTrigger = nullptr;

   public:
    OpMod(ExprRef<IntView> left, ExprRef<IntView> right)
        : left(std::move(left)), right(std::move(right)) {}

    OpMod(const OpMod& other) = delete;
    OpMod(OpMod&& other);
    ~OpMod() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPMOD_H_ */
