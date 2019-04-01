
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

    template <typename Func>
    inline bool changeValue(Func&& func) {
        Int oldValue = value;

        if (func() && value != oldValue) {
            notifyEntireValueChanged();
            return true;
        }
        return false;
    }
    void matchValueOf(IntView& other) {
        changeValue([&]() {
            value = other.value;
            return true;
        });
    }
    void standardSanityChecksForThisType() const;
};

struct IntViolationContext : public ViolationContext {
    enum class Reason { TOO_LARGE, TOO_SMALL };
    Reason reason;
    IntViolationContext(UInt parentViolation, Reason reason)
        : ViolationContext(parentViolation), reason(reason) {}
};
inline std::ostream& operator<<(std::ostream& os,
                                const IntViolationContext::Reason& r) {
    if (r == IntViolationContext::Reason::TOO_SMALL) {
        os << "TO_SMALL";
    } else {
        os << "TO_LARGE";
    }
    return os;
}
#endif /* SRC_TYPES_INT_H_ */
