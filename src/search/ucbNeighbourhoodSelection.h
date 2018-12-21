#ifndef SRC_SEARCH_UCB_NEIGHBOURHOODSELECTION_H
#define SRC_SEARCH_UCB_NEIGHBOURHOODSELECTION_H

#include <algorithm>
#include <cassert>
#include <climits>
#include <vector>
#include "search/model.h"
#include "search/solver.h"
#include "search/statsContainer.h"
#include "utils/random.h"
#define d_log(x)  // std::cout << x << std::endl;
class UcbNeighbourhoodSelection {
    double ucbExplorationBias;

   public:
    UcbNeighbourhoodSelection(bool,double ucbExplorationBias)
        : ucbExplorationBias(ucbExplorationBias) {}

    inline double ucbValue(double reward, double totalCost,
                           double neighbourhoodCost) {
        d_log("total cost " << totalCost << " nh cost " << neighbourhoodCost
                            << " reward=" << reward);
        return (reward / neighbourhoodCost) +
               std::sqrt((ucbExplorationBias * std::log(totalCost)) /
                         (neighbourhoodCost));
    }

    bool optimising(const State& state) {
        return state.model.optimiseMode != OptimiseMode::NONE &&
               state.model.getViolation() == 0;
    }
    inline double reward(const State& state, const NeighbourhoodStats& s) {
        return (optimising(state)) ? s.numberObjImprovements
                                   : s.numberVioImprovmenents;
    }
    inline double neighbourhoodCost(const State& state,
                                    const NeighbourhoodStats& s) {
        d_log("vioMinorNodeCount is " << s.vioMinorNodeCount);
        return (optimising(state)) ? s.minorNodeCount - s.vioMinorNodeCount
                                   : s.vioMinorNodeCount;
    }
    inline double totalCost(const State& state) {
        return (optimising(state))
                   ? state.stats.minorNodeCount - state.stats.vioMinorNodeCount
                   : state.stats.vioMinorNodeCount;
    }
    inline bool wasActivated(const State& state, const NeighbourhoodStats& s) {
        return (optimising(state))
                   ? s.numberActivations - s.numberVioActivations > 0
                   : s.numberVioActivations > 0;
    }

    template <typename ParentStrategy>
    inline void run(State& state, ParentStrategy&& parentStrategy) {
        double bestUCTValue = -(std::numeric_limits<double>::max());
        bool allNeighbourhoodsTryed = true;
        std::vector<int> bestNeighbourhoods;
        // changed so that if multiple neighbourhoods have the same best UCb
        // value, a random one is selected
        auto& nhStats = state.stats.neighbourhoodStats;
        d_log("optimise mode " << optimising(state));
        for (size_t i = 0; i < nhStats.size(); i++) {
            if (!wasActivated(state, nhStats[i])) {
                d_log(i << " not activated.");
                ;
                if (allNeighbourhoodsTryed) {
                    allNeighbourhoodsTryed = false;
                    bestNeighbourhoods.clear();
                }
                bestNeighbourhoods.push_back(i);
            }
            if (allNeighbourhoodsTryed) {
                double currentUCBValue =
                    ucbValue(reward(state, nhStats[i]), totalCost(state),
                             neighbourhoodCost(state, nhStats[i]));
                d_log("Ucb value of " << i << "=" << currentUCBValue);
                if (currentUCBValue > bestUCTValue) {
                    d_log("best");
                    bestUCTValue = currentUCBValue;
                    bestNeighbourhoods.clear();
                    bestNeighbourhoods.push_back(i);
                } else if (currentUCBValue == bestUCTValue) {
                    d_log("equal");
                    bestNeighbourhoods.push_back(i);
                }
            }
        }

        if (bestNeighbourhoods.empty()) {
            std::cout << "UCBNeighbourhoodSelection: could not activate a "
                         "neighbourhood.\n";
            throw EndOfSearchException();
        }
        d_log("best neighbourhoods: " << bestNeighbourhoods);
        size_t chosenNeighbourhood = bestNeighbourhoods[globalRandom<size_t>(
            0, bestNeighbourhoods.size() - 1)];
        d_log("chosen neighbourhood: " << chosenNeighbourhood);

        state.runNeighbourhood(chosenNeighbourhood, [&](const auto& result) {
            return parentStrategy(result);
        });
    }
};

#endif /* SRC_SEARCH_UCB_NEIGHBOURHOODSELECTION_H */
