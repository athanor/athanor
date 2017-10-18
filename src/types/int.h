
#ifndef SRC_TYPES_INT_H_
#define SRC_TYPES_INT_H_
#include <numeric>
#include <utility>
#include <vector>

#include "forwardDecls/typesAndDomains.h"
#include "operators/intProducing.h"

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

struct IntValue : public ValBase {
    int64_t value;
    std::vector<std::shared_ptr<IntTrigger>> triggers;

    template <typename Func>
    inline void changeValue(Func&& func) {
        for (std::shared_ptr<IntTrigger>& t : triggers) {
            t->possibleValueChange(value);
        }
        func();
        for (std::shared_ptr<IntTrigger>& t : triggers) {
            t->valueChanged(value);
        }
    }
};

#endif /* SRC_TYPES_INT_H_ */
