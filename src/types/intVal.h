
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
    // structs for possible return values of findContainingBound function below
    struct FoundBound {
        size_t index;
        FoundBound(size_t index) : index(index) {}
    };

    struct BetweenBounds {
        size_t lower;
        size_t upper;
        BetweenBounds(size_t lower, size_t upper)
            : lower(lower), upper(upper) {}
    };
    struct OutOfBoundsSmall {};
    struct OutOfBoundsLarge {};
    typedef mpark::variant<FoundBound, BetweenBounds, OutOfBoundsLarge,
                           OutOfBoundsSmall>
        ContainingBound;
    inline ContainingBound findContainingBound(Int value) const {
        if (bounds.empty()) {
            std::cerr << "Error: empty domain.'n";
            abort();
        }

        if (value < bounds.front().first) {
            return OutOfBoundsSmall();
        }
        if (value > bounds.back().second) {
            return OutOfBoundsLarge();
        }
        for (size_t i = 0; i < bounds.size(); i++) {
            const auto& bound = bounds[i];
            if (bound.first <= value && bound.second >= value) {
                return FoundBound(i);
            }
            if (i + 1 < bounds.size()) {
                // has next bound
                auto& nextBound = bounds[i + 1];
                if (value > bound.second && value < nextBound.first) {
                    return BetweenBounds(i, i + 1);
                }
            }
        }
        // should never reach here
        shouldNotBeCalledPanic;
    }

    inline bool containsValue(Int value) const {
        auto c = findContainingBound(value);
        return mpark::get_if<FoundBound>(&c);
    }
};

struct IntValue : public IntView, ValBase {
    const IntDomain* domain = NULL;

    inline bool domainContainsValue(Int value) {
        debug_code(assert(domain));
        return domain->containsValue(value);
    }

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
