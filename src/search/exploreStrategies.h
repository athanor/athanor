
#ifndef SRC_SEARCH_EXPLORESTRATEGIES_H_
#define SRC_SEARCH_EXPLORESTRATEGIES_H_
#include <cmath>
#include <deque>
#include "search/improveStrategies.h"
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/solver.h"
#include "search/statsContainer.h"

class RandomWalk : public SearchStrategy {
    std::shared_ptr<RandomNeighbourhood> nhSelector =
        std::make_shared<RandomNeighbourhood>();

   public:
    UInt64 numberMovesToTake = 0;
    bool allowViolations;

    RandomWalk(bool allowViolations) : allowViolations(allowViolations) {}
    void run(State& state, bool) {
        UInt startViolation = state.model.getViolation();
        UInt allowedViolationBackOff = numberMovesToTake;
        UInt64 numberIterations = 0;
        while (numberIterations <= numberMovesToTake) {
            state.runNeighbourhood(
                nhSelector->nextNeighbourhood(state), [&](const auto& result) {
                    if (!result.foundAssignment) {
                        return false;
                    }
                    bool allowed = result.model.getViolation() <
                                   startViolation + allowedViolationBackOff;
                    numberIterations += allowed;
                    return allowed;
                });
        }
    }
};

class ExplorationUsingViolationBackOff : public SearchStrategy {
    std::shared_ptr<SearchStrategy> climbStrategy;
    std::shared_ptr<RandomNeighbourhood> exploreStrategy =
        std::make_shared<RandomNeighbourhood>();

   public:
    ExplorationUsingViolationBackOff(
        std::shared_ptr<SearchStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}
    void run(State& state, bool) {
        bool explorationSupported =
            state.model.optimiseMode != OptimiseMode::NONE;
        if (!explorationSupported) {
            std::cout << "[warning]: This exploration strategy does not work "
                         "with satisfaction "
                         "problems.  Will not enter explore mode using this "
                         "strategy.\n";
        }
        while (true) {
            climbStrategy->run(state, !explorationSupported);
            if (!explorationSupported) {
                signalEndOfSearch();
            }
            explore(state);
        }
    }

    void backOff(State& state, const Objective& objToBeat,
                 ExponentialIncrementer<UInt>& violationBackOff) {
        UInt64 iterationsWithoutChange = 0;
        while (objToBeat <= state.model.getObjective()) {
            bool allowed = false;
            state.runNeighbourhood(
                exploreStrategy->nextNeighbourhood(state),
                [&](const auto& result) {
                    if (!result.foundAssignment) {
                        return false;
                    }
                    allowed = result.model.getViolation() <=
                                  violationBackOff.getValue() &&
                              result.model.getObjective() < objToBeat;
                    return allowed;
                });
            if (!allowed) {
                ++iterationsWithoutChange;
                if (iterationsWithoutChange % improveStratPeakIterations == 0) {
                    violationBackOff.increment();
                    iterationsWithoutChange = 0;
                }
            }
        }
    }

    void randomStep(State& state) {
        bool stepTaken = false;
        while (!stepTaken) {
            state.runNeighbourhood(exploreStrategy->nextNeighbourhood(state),
                                   [&](const auto& result) {
                                       stepTaken = result.foundAssignment;
                                       return stepTaken;
                                   });
        }
    }

    bool repair(State& state, const Objective& objToBeat) {
        UInt64 iterationsWithoutChange = 0;
        while (state.model.getViolation() != 0) {
            bool allowed = false;
            state.runNeighbourhood(
                exploreStrategy->nextNeighbourhood(state),
                [&](const auto& result) {
                    if (!result.foundAssignment) {
                        return false;
                    }
                    allowed = result.getDeltaViolation() <= 0 &&
                              result.model.getObjective() < objToBeat;
                    return allowed;
                });
            if (!allowed) {
                ++iterationsWithoutChange;
                if (iterationsWithoutChange == improveStratPeakIterations) {
                    return false;
                }
            }
        }
        return true;
    }
    void explore(State& state) {
        auto objToBeat = state.model.getObjective();
        ExponentialIncrementer<UInt> violationBackOff(1, 1.1);
        while (true) {
            backOff(state, objToBeat, violationBackOff);
            bool repaired = repair(state, objToBeat);
            if (repaired) {
                return;
            } else {
                randomStep(state);
                violationBackOff.increment();
            }
        }
    }
};
template <typename Quality, typename QualityFunc>
class ExplorationUsingRandomWalkImpl {
    static constexpr UInt64 MAX_NUMBER_RANDOM_MOVES = 500;
    static constexpr double baseValue = 10;
    static constexpr double multiplier = 1.3;
    std::shared_ptr<SearchStrategy> climbStrategy;
    RandomWalk randomWalkStrategy = RandomWalk(false);
    ExponentialIncrementer<UInt64> numberRandomMoves =
        ExponentialIncrementer<UInt64>(baseValue, multiplier);
    QualityFunc qualityFunc;
    Quality bestQuality;

   public:
    ExplorationUsingRandomWalkImpl(
        std::shared_ptr<SearchStrategy> climbStrategy, State& state,
        QualityFunc qualityFunc)
        : climbStrategy(std::move(climbStrategy)),
          qualityFunc(std::move(qualityFunc)),
          bestQuality(this->qualityFunc(state)) {
        randomWalkStrategy.allowViolations = true;
    }

    bool exploreAndClimb(State& state) {
        Quality lastQuality = qualityFunc(state);
        randomWalkStrategy.numberMovesToTake = numberRandomMoves.getValue();
        randomWalkStrategy.run(state, false);
        if (numberRandomMoves.getValue() > MAX_NUMBER_RANDOM_MOVES) {
            // reset
            bestQuality = qualityFunc(state);
            numberRandomMoves.reset(baseValue, multiplier);
        }
        climbStrategy->run(state, false);
        if (qualityFunc(state) < bestQuality) {
            // smaller is better
            bestQuality = qualityFunc(state);
            numberRandomMoves.reset(baseValue, multiplier);
        } else {
            numberRandomMoves.increment();
        }
        return qualityFunc(state) < lastQuality;
    }
};
template <typename Quality, typename QualityFunc>
auto makeExplorationUsingRandomWalkImpl(
    std::shared_ptr<SearchStrategy> climbStrategy, State& state,
    QualityFunc qualityFunc) {
    return ExplorationUsingRandomWalkImpl<Quality, QualityFunc>(
        climbStrategy, state, std::move(qualityFunc));
}
class ExplorationUsingRandomWalk : public SearchStrategy {
    std::shared_ptr<SearchStrategy> climbStrategy;

   public:
    ExplorationUsingRandomWalk(std::shared_ptr<SearchStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}

    void run(State& state, bool) {
        climbStrategy->run(state, false);
        {
            auto explorer = makeExplorationUsingRandomWalkImpl<UInt>(
                climbStrategy, state,
                [](State& state) { return state.model.getViolation(); });
            while (state.model.getViolation() > 0) {
                explorer.exploreAndClimb(state);
            }
        }

        {
            auto explorer = makeExplorationUsingRandomWalkImpl<Objective>(
                climbStrategy, state,
                [](State& state) { return state.model.getObjective(); });
            while (true) {
                explorer.exploreAndClimb(state);
            }
        }
    }
};
#endif /* SRC_SEARCH_EXPLORESTRATEGIES_H_ */
