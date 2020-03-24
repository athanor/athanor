
#ifndef SRC_SEARCH_NEIGHBOURHOODSEARCHSTRATEGIES_H_
#define SRC_SEARCH_NEIGHBOURHOODSEARCHSTRATEGIES_H_
#include "search/solver.h"
#include "search/statsContainer.h"
class NeighbourhoodSearchStrategy {
   public:
    typedef std::function<bool(const NeighbourhoodResult&)> Callback;
    virtual ~NeighbourhoodSearchStrategy() {}
    virtual void search(State& state, size_t neighbourhood,
                        Callback callback) = 0;
};
class ApplyOnce : public NeighbourhoodSearchStrategy {
   public:
    void search(State& state, size_t neighbourhood, Callback callback) {
        state.runNeighbourhood(neighbourhood, callback);
    }
};

class FirstAtLeastEqual : public NeighbourhoodSearchStrategy {
    size_t maxNumberAttempts;

   public:
    FirstAtLeastEqual(size_t maxNumberAttempts)
        : maxNumberAttempts(maxNumberAttempts) {}
    void search(State& state, size_t neighbourhood, Callback callback) {
        size_t attempts = 0;
        while (attempts < maxNumberAttempts) {
            ++attempts;
            state.runNeighbourhood(neighbourhood, [&](const auto& result) {
                bool atLeastAsGood = (result.statsMarkPoint.lastViolation != 0)
                                         ? result.getDeltaViolation() <= 0
                                         : result.model.getViolation() == 0 &&
                                               result.objectiveBetterOrEqual();
                if (atLeastAsGood || attempts == maxNumberAttempts) {
                    return callback(result);
                }
                return false;
            });
        }
    }
};

#endif /* SRC_SEARCH_NEIGHBOURHOODSEARCHSTRATEGIES_H_ */
