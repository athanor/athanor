
#ifndef SRC_OPERATORS_INTRANGE_H_
#define SRC_OPERATORS_INTRANGE_H_
#include "operators/simpleOperator.h"
#include "types/int.h"
#include "types/sequence.h"
struct IntRange;
template <>
struct OperatorTrates<IntRange> {
    template <bool isLeft>
    struct Trigger;
    typedef Trigger<true> LeftTrigger;
    typedef Trigger<false> RightTrigger;
};

struct IntRange : public SimpleBinaryOperator<SequenceView, IntView, IntRange> {
    Int cachedLower;
    Int cachedUpper;
    bool lowerExclusive;
    bool upperExclusive;

    IntRange(ExprRef<IntView> left, ExprRef<IntView> right,
             bool lowerExclusive = false, bool upperExclusive = false)
        : SimpleBinaryOperator<SequenceView, IntView, IntRange>(
              std::move(left), std::move(right)),
          lowerExclusive(lowerExclusive),
          upperExclusive(upperExclusive) {}
    inline Int lower() { return cachedLower - lowerExclusive; }
    inline Int upper() { return cachedUpper + upperExclusive; }

    void setInnerType() { this->members.emplace<ExprRefVec<IntView>>(); }
    void reevaluate();
    void updateVarViolations(UInt parentViolation,
                                    ViolationContainer& vioDesc) final;
    void copy(IntRange& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
};
#endif /* SRC_OPERATORS_INTRANGE_H_ */
