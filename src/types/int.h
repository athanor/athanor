
#ifndef SRC_TYPES_INT_H_
#define SRC_TYPES_INT_H_
#include <numeric>
#include <utility>
#include <vector>
#include "triggers/intTrigger.h"
#include "utils/ignoreUnused.h"

#include "base/base.h"

struct IntView : public ExprInterface<IntView>,
                 public TriggerContainer<IntView> {
    Int value;

    inline void initFrom(IntView& other) { value = other.value; }

    template <typename Func>
    inline bool changeValue(Func&& func) {
        Int oldValue = value;

        if (func() && value != oldValue) {
            visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
            return true;
        }
        return false;
    }
    void standardSanityChecksForThisType() const;
};

struct IntViolationContext : public ViolationContext {
    enum class Reason { TOO_LARGE, TOO_SMALL };
    Reason reason;
    IntViolationContext(UInt parentViolation, Reason reason)
        : ViolationContext(parentViolation), reason(reason) {}
};
#endif /* SRC_TYPES_INT_H_ */
