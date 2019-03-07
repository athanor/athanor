#ifndef SRC_SEARCH_UCB_NEIGHBOURHOODSELECTOR_H
#define SRC_SEARCH_UCB_NEIGHBOURHOODSELECTOR_H
#include <algorithm>
#include <cassert>
#include <climits>
#include <vector>
#include "search/endOfSearchException.h"
#include "utils/random.h"
extern double DEFAULT_UCB_EXPLORATION_BIAS;
template <typename Derived>
class UcbSelector {
    double ucbExplorationBias;

    Derived& derived() { return *static_cast<Derived*>(this); }

   public:
    UcbSelector(double ucbExplorationBias)
        : ucbExplorationBias(ucbExplorationBias) {}

    inline double ucbValue(double reward, double totalCost,
                           double individualCost) {
        return (reward / individualCost) +
               std::sqrt((ucbExplorationBias * std::log(totalCost)) /
                         (individualCost));
    }

    size_t next() {
        double bestUCTValue = -(std::numeric_limits<double>::max());
        bool allOptionsTryed = true;
        std::vector<int> bestOptions;
        for (size_t i = 0; i < derived().numberOptions(); i++) {
            if (!derived().wasActivated(i)) {
                if (allOptionsTryed) {
                    allOptionsTryed = false;
                    bestOptions.clear();
                }
                bestOptions.push_back(i);
            }
            if (allOptionsTryed) {
                double currentUCBValue =
                    ucbValue(derived().reward(i), derived().totalCost(),
                             derived().individualCost(i));
                if (currentUCBValue > bestUCTValue) {
                    bestUCTValue = currentUCBValue;
                    bestOptions.clear();
                    bestOptions.push_back(i);
                } else if (currentUCBValue == bestUCTValue) {
                    bestOptions.push_back(i);
                }
            }
        }

        if (bestOptions.empty()) {
            std::cout << "UCBOptionSelection: could not activate a "
                         "option.\n";
            throw EndOfSearchException();
        }
        size_t chosenOption =
            bestOptions[globalRandom<size_t>(0, bestOptions.size() - 1)];
        return chosenOption;
    }
};

class StandardUcbSelector : public UcbSelector<StandardUcbSelector> {
    std::vector<bool> _wasActivated;
    std::vector<double> rewards;
    std::vector<double> costs;
    double _totalCost = 0;

   public:
    StandardUcbSelector(size_t numberOptions, double explorationBias)
        : UcbSelector<StandardUcbSelector>(explorationBias),
          _wasActivated(numberOptions, false),
          rewards(numberOptions, 0),
          costs(numberOptions, 0) {}

    double totalCost() { return _totalCost; }
    double reward(size_t i) { return rewards[i]; }
    double individualCost(size_t i) { return costs[i]; }
    bool wasActivated(size_t i) { return _wasActivated[i]; }
    size_t numberOptions() { return _wasActivated.size(); }
    void reportResult(size_t option, double reward, double cost) {
        rewards[option] += reward;
        costs[option] += cost;
        _totalCost += cost;
        _wasActivated[option] = true;
    }
};
#endif /* SRC_SEARCH_UCB_NEIGHBOURHOODSELECTOR_H */
