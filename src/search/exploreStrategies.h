
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

class ExplorationUsingRandomWalk : public SearchStrategy {
    static constexpr UInt64 MAX_NUMBER_RANDOM_MOVES = 500;
    static constexpr double baseValue = 10;
    static constexpr double multiplier = 1.3;
    std::shared_ptr<SearchStrategy> climbStrategy;
    RandomWalk randomWalkStrategy = RandomWalk(false);

   public:
    ExplorationUsingRandomWalk(std::shared_ptr<SearchStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}
    template <typename Func1, typename Func2>
    void runImpl(State& state, Func1&& quality, Func2&& finished) {
        auto bestQuality = quality(state);
        ExponentialIncrementer<UInt64> numberRandomMoves(baseValue, multiplier);
        while (true) {
            climbStrategy->run(state, false);
            if (finished(state)) {
                return;
            }

            if (quality(state) < bestQuality) {
                // smaller is better
                bestQuality = quality(state);
                numberRandomMoves.reset(baseValue, multiplier);
            } else {
                numberRandomMoves.increment();
            }
            randomWalkStrategy.numberMovesToTake = numberRandomMoves.getValue();
            randomWalkStrategy.run(state, false);
            if (numberRandomMoves.getValue() > MAX_NUMBER_RANDOM_MOVES) {
                // reset
                bestQuality = quality(state);
                numberRandomMoves.reset(baseValue, multiplier);
            }
        }
    }

    void run(State& state, bool) {
        randomWalkStrategy.allowViolations = true;
        runImpl(
            state, [](State& state) { return state.model.getViolation(); },
            [](State& state) { return state.model.getViolation() == 0; });
        //        randomWalkStrategy.allowViolations = false;
        runImpl(
            state, [](State& state) { return state.model.getObjective(); },
            [](State&) { return false; });
    }
};

#endif /* SRC_SEARCH_EXPLORESTRATEGIES_H_ */
