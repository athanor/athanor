
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


class UcbNeighbourhoodSelection  {
      double ucbExplorationBias = 2;
public:

    inline double ucbValue(double reward, double totalCost,
                           double neighbourhoodCost) {
        return (reward / neighbourhoodCost) +
        std::sqrt((ucbExplorationBias * std::log(totalCost)) /
                  (neighbourhoodCost));
    }


    bool optimising(const State& state) {
        return state.model.optimiseMode != OptimiseMode::NONE && state.model.getViolation() == 0;
    }
        inline double reward(const State& state, const NeighbourhoodStats& s) { return (optimising(state))? s.numberObjImprovements : s.numberVioImprovmenents; }
        inline double neighbourhoodCost(const State& state, const NeighbourhoodStats& s) { return (optimising(state))? s.minorNodeCount - s.vioMinorNodeCount: s.vioMinorNodeCount; }
    inline double totalCost(const State& state) { return (optimising(state))? state.stats.minorNodeCount -state.stats.vioMinorNodeCount: state.stats.vioMinorNodeCount; }
                                                              inline bool  wasActivated(const State& state, const NeighbourhoodStats& s) {
                                                                  return (optimising(state))? s.numberActivations - s.numberVioActivations > 0 : s.numberVioActivations > 0;
                                                              }

    template <typename ParentStrategy>
    inline void run(State& state, ParentStrategy&& parentStrategy) {
        double bestUCTValue = -(std::numeric_limits<double>::max());
        bool allNeighbourhoodsTryed = true;
        std::vector<int> bestNeighbourhoods;
        // changed so that if multiple neighbourhoods have the same best UCb value, a random one is
        // selected
        auto& nhStats = state.stats.neighbourhoodStats;
        for(size_t i = 0; i < nhStats.size(); i++) {
            if(!wasActivated(state,nhStats[i])) {
                if(allNeighbourhoodsTryed) {
                        allNeighbourhoodsTryed = false;
                        bestNeighbourhoods.clear();
                    }
                    bestNeighbourhoods.push_back(i);
                } else {
                    double currentUCBValue = ucbValue(reward(state, nhStats[i]), totalCost(state),
                                                      neighbourhoodCost(state,nhStats[i]));
                    if(currentUCBValue > bestUCTValue) {
                        bestUCTValue = currentUCBValue;
                        bestNeighbourhoods.clear();
                        bestNeighbourhoods.push_back(i);
                    } else if(currentUCBValue == bestUCTValue) {
                        bestNeighbourhoods.push_back(i);
                    }
                }
            }

        if(bestNeighbourhoods.empty()) {
            std::cout << "UCBNeighbourhoodSelection: could not activate a neighbourhood.\n";
            throw EndOfSearchException();
        }
            size_t chosenNeighbourhood = globalRandom<size_t>(0,bestNeighbourhoods.size() -1);
            state.runNeighbourhood(chosenNeighbourhood,
                                   [&](const auto& result) { return parentStrategy(result); });

    }
};

#endif /* SRC_SEARCH_UCB_NEIGHBOURHOODSELECTION_H */
