
#ifndef SRC_TYPES_INTVAL_H_
#define SRC_TYPES_INTVAL_H_
#include <numeric>
#include <utility>
#include <vector>
#include "types/int.h"
#include "utils/ignoreUnused.h"

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

struct IntValue : public IntView, ValBase {
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<IntView> deepCopyForUnrollImpl(
        const ExprRef<IntView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<IntView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_INTVAL_H_ */
