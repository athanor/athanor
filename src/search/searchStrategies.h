
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <cmath>
#include <deque>
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/solver.h"
#include "search/statsContainer.h"

u_int64_t ALLOWED_ITERATIONS_OF_INACTIVITY = 1000;

template <typename Strategy>
class HillClimbing {
    std::shared_ptr<Strategy> strategy;
    ;

   public:
    HillClimbing(std::shared_ptr<Strategy> strategy)
        : strategy(std::move(strategy)) {}
    void run(State& state, bool isOuterMostStrategy) {
        u_int64_t iterationsAtPeak = 0;
        while (true) {
            strategy->run(state, [&](const auto& result) {
                if (!result.foundAssignment) {
                    return false;
                }
                bool allowed = false;
                if (result.statsMarkPoint.lastViolation != 0) {
                    allowed = result.getDeltaViolation() <= 0;
                } else {
                    allowed = result.model.getViolation() == 0 &&
                              result.getDeltaObjective() <= 0;
                }
                if (!isOuterMostStrategy && !allowed &&
                    state.model.getViolation() == 0) {
                    ++iterationsAtPeak;
                }
                return allowed;
            });
            if (!isOuterMostStrategy &&
                iterationsAtPeak > ALLOWED_ITERATIONS_OF_INACTIVITY) {
                break;
            }
        }
    }
};

template <typename Strategy>
auto makeHillClimbing(std::shared_ptr<Strategy> strategy) {
    return std::make_shared<HillClimbing<Strategy>>(std::move(strategy));
}

class RandomWalk {
    std::shared_ptr<RandomNeighbourhood> nhSelector = std::make_shared<RandomNeighbourhood>();
    bool allowViolations;

   public:
    u_int64_t numberMovesToTake = 0;
    RandomWalk(bool allowViolations) : allowViolations(allowViolations) {}
    void run(State& state, bool) {
        u_int64_t numberIterations = 0;
        while (numberIterations <= numberMovesToTake) {
            nhSelector->run(state, [&](const auto& result) {
                if (!result.foundAssignment) {
                    return false;
                }
                bool allowed =
                    allowViolations || result.model.getViolation() == 0;
                numberIterations += allowed;
                return allowed;
            });
        }
    }
};

template <typename Integer>
class ExponentialIncrementer {
    double value;
    double exponent;

   public:
    ExponentialIncrementer(double initialValue, double exponent)
        : value(initialValue), exponent(exponent) {}
    void reset(double initialValue, double exponent) {
        this->value = initialValue;
        this->exponent = exponent;
    }
    Integer getValue() { return std::round(value); }
    void increment() {
        if (value > ((u_int64_t)1) << 32) {
            value = 1;
        }
        value *= exponent;
    }
};

template <typename ClimbStrategy, typename ExploreStrategy>
class ExplorationUsingViolationBackOff {
    std::shared_ptr<ClimbStrategy> climbStrategy;
    std::shared_ptr<ExploreStrategy> exploreStrategy;

   public:
    ExplorationUsingViolationBackOff(
        std::shared_ptr<ClimbStrategy> climbStrategy,
        std::shared_ptr<ExploreStrategy> exploreStrategy)
        : climbStrategy(std::move(climbStrategy)),
          exploreStrategy(std::move(exploreStrategy)) {}
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
                throw EndOfSearchException();
            }
            explore(state);
        }
    }

    void backOff(State& state, Int objToBeat,
                 ExponentialIncrementer<UInt>& violationBackOff) {
        u_int64_t iterationsWithoutChange = 0;
        while (state.model.getObjective() >= objToBeat) {
            bool allowed = false;
            exploreStrategy->run(state, [&](const auto& result) {
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
                if (iterationsWithoutChange %
                        ALLOWED_ITERATIONS_OF_INACTIVITY ==
                    0) {
                    violationBackOff.increment();
                    iterationsWithoutChange = 0;
                }
            }
        }
    }

    void randomStep(State& state) {
        bool stepTaken = false;
        while (!stepTaken) {
            exploreStrategy->run(state, [&](const auto& result) {
                stepTaken = result.foundAssignment;
                return stepTaken;
            });
        }
    }

    bool repair(State& state, Int objToBeat) {
        u_int64_t iterationsWithoutChange = 0;
        while (state.model.getViolation() != 0) {
            bool allowed = false;
            exploreStrategy->run(state, [&](const auto& result) {
                if (!result.foundAssignment) {
                    return false;
                }
                allowed = result.getDeltaViolation() <= 0 &&
                          result.model.getObjective() < objToBeat;
                return allowed;
            });
            if (!allowed) {
                ++iterationsWithoutChange;
                if (iterationsWithoutChange ==
                    ALLOWED_ITERATIONS_OF_INACTIVITY) {
                    return false;
                }
            }
        }
        return true;
    }
    void explore(State& state) {
        int objToBeat = state.model.getObjective();
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

template <typename ClimbStrategy, typename ExploreStrategy>
auto makeExplorationUsingViolationBackOff(
    std::shared_ptr<ClimbStrategy> climbStrategy,
    std::shared_ptr<ExploreStrategy> exploreStrategy) {
    return std::make_shared<
        ExplorationUsingViolationBackOff<ClimbStrategy, ExploreStrategy>>(
        std::move(climbStrategy), std::move(exploreStrategy));
}

template <typename ClimbStrategy>
class ExplorationUsingRandomWalk {
    std::shared_ptr<ClimbStrategy> climbStrategy;
    RandomWalk randomWalkStrategy = RandomWalk(false);

   public:
    ExplorationUsingRandomWalk(std::shared_ptr<ClimbStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}
    void run(State& state, bool) {
        double baseValue = 10, multiplier = 1.3;
        bool explorationSupported =
            state.model.optimiseMode != OptimiseMode::NONE;
        if (!explorationSupported) {
            std::cout << "[warning]: This exploration strategy does not work "
                         "with satisfaction "
                         "problems.  Will not enter explore mode using this "
                         "strategy.\n";
        }
        do {
            climbStrategy->run(state, !explorationSupported);
        } while (state.model.getViolation() != 0);
        if (!explorationSupported) {
            throw EndOfSearchException();
        }

        Int bestObj = state.model.getObjective();
        ExponentialIncrementer<u_int64_t> numberRandomMoves(baseValue,
                                                            multiplier);
        while (true) {
            randomWalkStrategy.numberMovesToTake = numberRandomMoves.getValue();
            randomWalkStrategy.run(state, false);
            climbStrategy->run(state, !explorationSupported);

            if (state.model.getObjective() < bestObj) {
                bestObj = state.model.getObjective();
                numberRandomMoves.reset(baseValue, multiplier);
            } else {
                numberRandomMoves.increment();
                if (numberRandomMoves.getValue() >
                    ALLOWED_ITERATIONS_OF_INACTIVITY) {
                    numberRandomMoves.reset(baseValue, multiplier);
                }
            }
        }
    }
};
template <typename ClimbStrategy>
auto makeExplorationUsingRandomWalk(
    std::shared_ptr<ClimbStrategy> climbStrategy) {
    return std::make_shared<ExplorationUsingRandomWalk<ClimbStrategy>>(
        climbStrategy);
}

#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
