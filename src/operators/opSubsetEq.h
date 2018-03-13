
#ifndef SRC_OPERATORS_OPSETINTERSECT_H_
#define SRC_OPERATORS_OPSETINTERSECT_H_
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "types/set.h"
struct OpSubsetEq : public BoolView {
    struct LeftSetTrigger;
    struct RightSetTrigger;
    ExprRef<SetView> left;
    ExprRef<SetView> right;
    std::shared_ptr<LeftSetTrigger> leftTrigger;
    std::shared_ptr<RightSetTrigger> rightTrigger;

    OpSubsetEq(ExprRef<SetView> leftIn, ExprRef<SetView> rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
    OpSubsetEq(const OpSubsetEq& other) = delete;
    OpSubsetEq(OpSubsetEq&& other);
    ~OpSubsetEq() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPSETINTERSECT_H_ */
