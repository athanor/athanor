
#ifndef SRC_TYPES_INT_H_
#define SRC_TYPES_INT_H_
#include <numeric>
#include <utility>
#include <vector>
#include "utils/ignoreUnused.h"

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

struct IntTrigger : public TriggerBase {};

struct IntView : public ExprInterface<IntView> {
    Int value;
    std::vector<std::shared_ptr<IntTrigger>> triggers;

    inline void initFrom(IntView& other) { value = other.value; }

    template <typename Func>
    inline bool changeValue(Func&& func) {
        Int oldValue = value;

        if (func() && value != oldValue) {
            std::swap(value, oldValue);
            visitTriggers([&](auto& t) { t->possibleValueChange(); }, triggers);
            std::swap(oldValue, value);
            visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
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
        ExprRef<IntView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

template <typename Child>
struct ChangeTriggerAdapter<IntTrigger, Child>
    : public IntTrigger, public ChangeTriggerAdapterBase<Child> {
    inline void possibleValueChange() final {
        this->forwardPossibleValueChange();
    }
    inline void valueChanged() final { this->forwardValueChanged(); }
};

#endif /* SRC_TYPES_INT_H_ */
