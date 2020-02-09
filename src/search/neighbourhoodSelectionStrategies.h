
#ifndef SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#define SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#include <algorithm>
#include <cassert>
#include <climits>
#include <vector>
#include "search/model.h"
#include "search/solver.h"
#include "search/statsContainer.h"
#include "search/ucbSelector.h"
#include "utils/random.h"
void signalEndOfSearch();
class InteractiveNeighbourhoodSelector {
   public:
    template <typename ParentStrategy>
    inline void run(State& state, ParentStrategy&& parentStrategy) {
        while (true) {
            std::cout << "select a neighbourhood, choices are:\n";
            for (size_t i = 0; i < state.model.neighbourhoods.size(); i++) {
                std::cout << (i + 1) << ": "
                          << state.model.neighbourhoods[i].name << std::endl;
            }
            int input;
            if (!(std::cin >> input)) {
                std::cout << "Error: expected integer.\n";
                std::cin.clear();
                std::cin.ignore(INT_MAX, '\n');
                continue;
            }
            if (input == -1) {
                std::cout << "Exiting...\n";
                signalEndOfSearch();
            }
            if (input < 1 || input > (int)state.model.neighbourhoods.size()) {
                std::cout << "Error: integer out of range.\n";
                continue;
            }
            bool accepted = false;
            state.runNeighbourhood(input - 1, [&](const auto& result) {
                accepted = parentStrategy(result);
                return accepted;
            });
            if (accepted) {
                std::cout << "change accepted\n";
            } else {
                std::cout << "change rejected\n";
            }
            break;
        }
    }
};

class RandomNeighbourhood {
   public:
    template <typename ParentStrategy>
    inline void run(State& state, ParentStrategy&& parentStrategy) {
        state.runNeighbourhood(
            nextNeighbourhood(state),
            [&](const auto& result) { return parentStrategy(result); });
    }
    inline size_t nextNeighbourhood(const State& state) {
        if (state.vioContainer.getTotalViolation() == 0) {
            return globalRandom<size_t>(0,
                                        state.model.neighbourhoods.size() - 1);
        } else {
            size_t biasRandomVar;
            while (true) {
                biasRandomVar = state.vioContainer.selectRandomVar(
                    state.model.variables.size() - 1);
                if (!state.model.varNeighbourhoodMapping[biasRandomVar]
                         .empty()) {
                    break;
                }
            }
            return state.model
                .varNeighbourhoodMapping[biasRandomVar][globalRandom<size_t>(
                    0,
                    state.model.varNeighbourhoodMapping[biasRandomVar].size() -
                        1)];
        }
    }
};

class UcbNeighbourhoodSelector : public UcbSelector<UcbNeighbourhoodSelector> {
    const State& state;
    bool includeMinorNodeCount;
    bool includeTriggerEventCount;

   public:
    UcbNeighbourhoodSelector(const State& state,
                                      double ucbExplorationBias,
                                      bool includeMinorNodeCount,
                                      bool includeTriggerEventCount)
        : UcbSelector<UcbNeighbourhoodSelector>(ucbExplorationBias),
          state(state),
          includeMinorNodeCount(includeMinorNodeCount),
          includeTriggerEventCount(includeTriggerEventCount) {}

    bool optimising() {
        return state.model.optimiseMode != OptimiseMode::NONE &&
               state.model.getViolation() == 0;
    }
    const NeighbourhoodStats& nhStats(size_t i) {
        return state.stats.neighbourhoodStats[i];
    }

    inline double reward(size_t i) {
        auto& s = nhStats(i);
        return (optimising()) ? s.numberObjImprovements
                              : s.numberVioImprovements;
    }
    inline double individualCost(size_t i) {
        auto& s = nhStats(i);
        UInt64 cost = s.numberActivations, vioCost = s.numberVioActivations;
        cost += int(includeMinorNodeCount) * s.minorNodeCount;
        vioCost += int(includeMinorNodeCount) * s.vioMinorNodeCount;
        cost += int(includeTriggerEventCount) * s.triggerEventCount;
        vioCost += int(includeTriggerEventCount) * s.vioTriggerEventCount;
        return (optimising()) ? cost - vioCost : vioCost;
    }
    inline double totalCost() {
        UInt64 cost = state.stats.numberIterations,
               vioCost = state.stats.numberVioIterations;
        cost += (includeMinorNodeCount)*state.stats.minorNodeCount;
        vioCost += int(includeMinorNodeCount) * state.stats.vioMinorNodeCount;
        cost += int(includeTriggerEventCount) * triggerEventCount;
        vioCost +=
            int(includeTriggerEventCount) * state.stats.vioTriggerEventCount;
        return (optimising()) ? cost - vioCost : vioCost;
    }
    inline bool wasActivated(size_t i) {
        auto& s = nhStats(i);
        return (optimising()) ? s.numberActivations - s.numberVioActivations > 0
                              : s.numberVioActivations > 0;
    }
    inline size_t numberOptions() {
        return state.stats.neighbourhoodStats.size();
    }
    template <typename ParentStrategy>
    inline void run(State& state, ParentStrategy&& parentStrategy) {
        size_t chosenNeighbourhood = next();

        state.runNeighbourhood(chosenNeighbourhood, [&](const auto& result) {
            return parentStrategy(result);
        });
    }
};

#endif /* SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_ */
