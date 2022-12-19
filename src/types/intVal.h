
#ifndef SRC_TYPES_INTVAL_H_
#define SRC_TYPES_INTVAL_H_
#include <numeric>
#include <utility>
#include <vector>

#include "types/int.h"
#include "utils/ignoreUnused.h"

inline auto intBound(Int a, Int b) { return std::make_pair(a, b); }
struct IntDomain {
    std::vector<std::pair<Int, Int>> bounds;
    UInt domainSize;
    IntDomain(std::vector<std::pair<Int, Int>> bounds)
        : bounds(normaliseBounds(std::move(bounds))),
          domainSize(calculateDomainSize(this->bounds)) {}

    inline std::shared_ptr<IntDomain> deepCopy(std::shared_ptr<IntDomain>&) {
        return std::make_shared<IntDomain>(*this);
    }

    static inline UInt calculateDomainSize(
        const std::vector<std::pair<Int, Int>>& bounds) {
        return std::accumulate(
            bounds.begin(), bounds.end(), UInt(0), [](UInt total, auto& range) {
                return total + (range.second - range.first) + 1;
            });
    }

    // sort and unify overlaps
    static std::vector<std::pair<Int, Int>> normaliseBounds(
        std::vector<std::pair<Int, Int>> bounds) {
        std::sort(bounds.begin(), bounds.end());
        std::remove_if(bounds.begin(), bounds.end(),
                       [](auto& b) { return b.second < b.first; });
        for (size_t i = 0; i < bounds.size() - 1; i++) {
            // merge if overlap or forms contiguous domain
            auto& bound = bounds[i];
            auto& nextBound = bounds[i + 1];
            if (bound.second >= nextBound.first - 1) {
                bound.second = std::max(bound.second, nextBound.second);
                bounds.erase(bounds.begin() + i + 1);
            }
        }
        return bounds;
    }

    void merge(const IntDomain& other) {
        bounds.insert(bounds.end(), other.bounds.begin(), other.bounds.end());
        bounds = normaliseBounds(std::move(bounds));
        domainSize = calculateDomainSize(bounds);
    }
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
    typedef lib::variant<FoundBound, BetweenBounds, OutOfBoundsLarge,
                         OutOfBoundsSmall>
        ContainingBound;
    inline ContainingBound findContainingBound(Int value) const {
        if (bounds.empty()) {
            myCerr << "Error: empty domain.'n";
            myAbort();
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
        return lib::get_if<FoundBound>(&c);
    }
};

struct IntValue : public IntView, ValBase {
    const IntDomain* domain = NULL;

    inline bool domainContainsValue(const IntView& view) {
        debug_code(assert(domain));
        return domain->containsValue(view.value);
    }

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<IntView> deepCopyForUnrollImpl(
        const ExprRef<IntView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<IntView>> optimiseImpl(ExprRef<IntView>&,
                                                   PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
    void hashChecksImpl() const final;
};

#endif /* SRC_TYPES_INTVAL_H_ */
