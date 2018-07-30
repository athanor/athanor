
#ifndef SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#define SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#include <algorithm>
#include <cassert>
#include <vector>
#include "search/model.h"
#include "search/solver.h"
#include "search/statsContainer.h"
#include "utils/random.h"

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
