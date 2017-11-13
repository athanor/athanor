
#ifndef SRC_OPERATORS_OPSETINTERSECT_H_
#define SRC_OPERATORS_OPSETINTERSECT_H_
#include "operators/operatorBase.h"
#include "types/set.h"
template <bool isLeft>
class OpSetIntersectTrigger;
struct OpSetIntersect : public SetView {
    SetReturning left;
    SetReturning right;
    std::shared_ptr<OpSetIntersectTrigger<true>> leftTrigger;
    std::shared_ptr<OpSetIntersectTrigger<false>> rightTrigger;

    OpSetIntersect(SetReturning leftIn, SetReturning rightIn)
        : left(std::move(leftIn)), right(std::move(rightIn)) {}
    OpSetIntersect(const OpSetIntersect& other) = delete;
    OpSetIntersect(OpSetIntersect&& other);
    ~OpSetIntersect() { stopTriggering(*this); }
};

#endif /* SRC_OPERATORS_OPSETINTERSECT_H_ */
