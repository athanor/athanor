
#ifndef SRC_OPERATORS_OPSETINTERSECT_H_
#define SRC_OPERATORS_OPSETINTERSECT_H_
#include "operators/operatorBase.h"
#include "types/bool.h"
#include "types/set.h"
struct OpSubset : public BoolView {
    struct LeftSetTrigger;
    struct RightSetTrigger;
    SetReturning left;
    SetReturning right;
    std::shared_ptr<LeftSetTrigger> leftTrigger;
    std::shared_ptr<RightSetTrigger> rightTrigger;

    OpSubset(SetReturning leftIn, SetReturning rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
    OpSubset(const OpSubset& other) = delete;
    OpSubset(OpSubset&& other);
    ~OpSubset() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPSETINTERSECT_H_ */
