#ifndef SRC_SEARCH_UCB_NEIGHBOURHOODSELECTOR_H
#define SRC_SEARCH_UCB_NEIGHBOURHOODSELECTOR_H

#include <algorithm>
#include <cassert>
#include <climits>
#include <vector>
#include "utils/random.h"
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
#endif /* SRC_SEARCH_UCB_NEIGHBOURHOODSELECTOR_H */
