
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
                throw EndOfSearchException();
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

class UcbWithTriggerCount : public UcbSelector<UcbWithTriggerCount> {
    const State& state;

   public:
    UcbWithTriggerCount(const State& state, double ucbExplorationBias)
        : UcbSelector<UcbWithTriggerCount>(ucbExplorationBias), state(state) {}

    bool optimising() const {
        return state.model.optimiseMode != OptimiseMode::NONE &&
               state.model.getViolation() == 0;
    }
    const NeighbourhoodStats& nhStats(size_t i) const {
        return state.stats.neighbourhoodStats[i];
    }

    inline double reward(size_t i) const {
        auto& s = nhStats(i);
        return (optimising()) ? s.numberObjImprovements
                              : s.numberVioImprovements;
    }

    inline double individualCost(size_t i) const {
        auto& s = nhStats(i);
        double penalty = 0;
        if (s.minorNodeCount > s.triggerEventCount) {
            UInt64 minorNodeCount =
                std::max(state.stats.minorNodeCount, ((UInt64)1));
            double penaltyRatio = ((double)triggerEventCount) / minorNodeCount;
            penalty = s.minorNodeCount * penaltyRatio;
        }
        UInt64 vioCost = s.vioMinorNodeCount + s.vioTriggerEventCount;
        UInt64 cost = s.minorNodeCount + s.triggerEventCount;
        return (optimising()) ? (cost + penalty) - vioCost : vioCost + penalty;
    }
    inline double totalCost() {
        double total = 0;
        for (size_t i = 0; i < numberOptions(); i++) {
            total += individualCost(i);
        }
        return total;
    }
    inline bool wasActivated(size_t i) const {
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

class UcbWithMinorNodeCount : public UcbSelector<UcbWithMinorNodeCount> {
    const State& state;

   public:
    UcbWithMinorNodeCount(const State& state, double ucbExplorationBias)
        : UcbSelector<UcbWithMinorNodeCount>(ucbExplorationBias),
          state(state) {}

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
        return (optimising()) ? s.minorNodeCount - s.vioMinorNodeCount
                              : s.vioMinorNodeCount;
    }
    inline double totalCost() {
        return (optimising())
                   ? state.stats.minorNodeCount - state.stats.vioMinorNodeCount
                   : state.stats.vioMinorNodeCount;
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
