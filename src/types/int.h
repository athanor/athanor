
#ifndef SRC_TYPES_INT_H_
#define SRC_TYPES_INT_H_
#include <numeric>
#include <utility>
#include <vector>

#include "base/base.h"

inline auto intBound(Int a, Int b) { return std::make_pair(a, b); }
struct IntDomain {
    const std::vector<std::pair<Int, Int>> bounds;
    const UInt domainSize;
    IntDomain(std::vector<std::pair<Int, Int>> bounds)
        : bounds(std::move(bounds)),
          domainSize(std::accumulate(
              this->bounds.begin(), this->bounds.end(), 0,
              [](UInt total, auto& range) {
                  return total + (range.second - range.first) + 1;
              })) {}
};

struct IntTrigger : public IterAssignedTrigger<IntView> {
    virtual void possibleValueChange(Int value) = 0;
    virtual void valueChanged(Int value) = 0;
};

struct IntView : public ExprInterface<IntView> {
    Int value;
    std::vector<std::shared_ptr<IntTrigger>> triggers;

    inline void initFrom(IntView& other) { value = other.value; }

    template <typename Func>
    inline bool changeValue(Func&& func) {
        Int oldValue = value;

        if (func() && value != oldValue) {
            visitTriggers([&](auto& t) { t->possibleValueChange(oldValue); },
                          triggers);
            visitTriggers([&](auto& t) { t->valueChanged(value); }, triggers);
            return true;
        }
        return false;
    }
};

struct IntValue : public IntView, ValBase {
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<IntView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
};

template <>
struct DefinedTrigger<IntValue> : public IntTrigger {
    ValRef<IntValue> val;
    DefinedTrigger(const ValRef<IntValue>& val) : val(val) {}
    inline void possibleValueChange(Int) final {}
    inline void valueChanged(Int value) final {
        val->changeValue([&]() {
            val->value = value;
            return true;
        });
    }

    void iterHasNewValue(const IntView&, const ExprRef<IntView>&) final {
        assert(false);
        abort();
    }
};

#endif /* SRC_TYPES_INT_H_ */
