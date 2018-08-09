
#ifndef SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#define SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#include <algorithm>
#include <cassert>
#include <climits>
#include <vector>
#include "search/model.h"
#include "search/solver.h"
#include "search/statsContainer.h"
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
        size_t numberNeighbourhoods = state.model.neighbourhoods.size();
        state.runNeighbourhood(
            globalRandom<size_t>(0, numberNeighbourhoods - 1),
            [&](const auto& result) { return parentStrategy(result); });
    }
};

class RandomNeighbourhoodWithViolation {
   public:
    template <typename ParentStrategy>
    inline void run(State& state, ParentStrategy&& parentStrategy) {
        updateViolations(state);
        state.runNeighbourhood(
            nextNeighbourhood(state),
            [&](const auto& result) { return parentStrategy(result); });
    }
    void updateViolations(State& state) {
        state.model.vioDesc.reset();
        if (state.model.csp->violation == 0) {
            return;
        }
        state.model.csp->updateVarViolations(0, state.model.vioDesc);
    }
    inline size_t nextNeighbourhood(const State& state) {
        if (state.model.vioDesc.getTotalViolation() == 0) {
            return globalRandom<size_t>(0,
                                        state.model.neighbourhoods.size() - 1);
        } else {
            size_t biasRandomVar = state.model.vioDesc.selectRandomVar(
                state.model.variables.size() - 1);
            return state.model
                .varNeighbourhoodMapping[biasRandomVar][globalRandom<size_t>(
                    0,
                    state.model.varNeighbourhoodMapping[biasRandomVar].size() -
                        1)];
        }
    }
};

#endif /* SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_ */
