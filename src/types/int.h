
#ifndef SRC_TYPES_INT_H_
#define SRC_TYPES_INT_H_
#include <numeric>
#include <utility>
#include <vector>

#include "types/base.h"

inline auto intBound(int64_t a, int64_t b) { return std::make_pair(a, b); }
struct IntDomain {
    const std::vector<std::pair<int64_t, int64_t>> bounds;
    const u_int64_t domainSize;
    IntDomain(std::vector<std::pair<int64_t, int64_t>> bounds)
        : bounds(std::move(bounds)),
          domainSize(std::accumulate(
              this->bounds.begin(), this->bounds.end(), 0,
              [](u_int64_t total, auto& range) {
                  return total + (range.second - range.first) + 1;
              })) {}
};

struct IntTrigger : public IterAssignedTrigger<IntValue> {
    typedef IntView View;
    virtual void possibleValueChange(int64_t value) = 0;
    virtual void valueChanged(int64_t value) = 0;
};

struct IntView {
    int64_t value;
    std::vector<std::shared_ptr<IntTrigger>> triggers;

    inline void initFrom(IntView& other) { value = other.value; }

    template <typename Func>
    inline bool changeValue(Func&& func) {
        int64_t oldValue = value;

        if (func() && value != oldValue) {
            visitTriggers([&](auto& t) { t->possibleValueChange(oldValue); },
                          triggers);
            visitTriggers([&](auto& t) { t->valueChanged(value); }, triggers);
            return true;
        }
        return false;
    }
};

struct IntValue : public IntView, ValBase {};

template <>
struct DefinedTrigger<IntValue> : public IntTrigger {
    ValRef<IntValue> val;
    DefinedTrigger(const ValRef<IntValue>& val) : val(val) {}
    inline void possibleValueChange(int64_t) final {}
    inline void valueChanged(int64_t value) final {
        val->changeValue([&]() {
            val->value = value;
            return true;
        });
    }

    void iterHasNewValue(const IntValue&, const ValRef<IntValue>&) final {
        assert(false);
        abort();
    }
};

#endif /* SRC_TYPES_INT_H_ */
