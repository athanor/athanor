
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <cmath>
#include <deque>
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/solver.h"
#include "search/statsContainer.h"

u_int64_t DEFAULT_PEAK_ITERATIONS = 1000;

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


template <typename Strategy>
class HillClimbing {
    std::shared_ptr<Strategy> strategy;
public:
    ExponentialIncrementer<u_int64_t> allowedIterationsAtPeak = ExponentialIncrementer<u_int64_t>(DEFAULT_PEAK_ITERATIONS,1);
    //default to no increment

   public:
    HillClimbing(std::shared_ptr<Strategy> strategy)
        : strategy(std::move(strategy)) {}
    void run(State& state, bool isOuterMostStrategy) {
        u_int64_t iterationsAtPeak = 0;
        while (true) {
            bool allowed = false, strictImprovement = false;
            strategy->run(state, [&](const auto& result) {
                if (result.foundAssignment) {
                    if (result.statsMarkPoint.lastViolation != 0) {
                        allowed = result.getDeltaViolation() <= 0;
                        strictImprovement = result.getDeltaViolation() < 0;
                    } else {
                        allowed = result.model.getViolation() == 0 &&
                                  result.getDeltaObjective() <= 0;
                        strictImprovement =
                            allowed && result.getDeltaObjective() < 0;
                    }
                }
                return allowed;
            });
            if (!isOuterMostStrategy && !strictImprovement) {
                ++iterationsAtPeak;
                if (iterationsAtPeak > allowedIterationsAtPeak.getValue()) {
                    break;
                }
            }
        }
    }
};

template <typename Strategy>
auto makeHillClimbing(std::shared_ptr<Strategy> strategy) {
    return std::make_shared<HillClimbing<Strategy>>(std::move(strategy));
}

class RandomWalk {
    std::shared_ptr<RandomNeighbourhood> nhSelector =
        std::make_shared<RandomNeighbourhood>();

   public:
    u_int64_t numberMovesToTake = 0;
    bool allowViolations;

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
                        DEFAULT_PEAK_ITERATIONS ==
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
                    DEFAULT_PEAK_ITERATIONS) {
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
    static constexpr double baseValue = 10;
    static constexpr double multiplier = 1.3;

    std::shared_ptr<ClimbStrategy> climbStrategy;
    RandomWalk randomWalkStrategy = RandomWalk(false);

   public:
    ExplorationUsingRandomWalk(std::shared_ptr<ClimbStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}
    template <typename Func1, typename Func2>
    void runImpl(State& state, Func1&& quality, Func2&& finished) {
        climbStrategy->allowedIterationsAtPeak.reset(1000,1.3);
        climbStrategy->run(state, false);
        auto bestQuality = quality(state);
        ExponentialIncrementer<u_int64_t> numberRandomMoves(baseValue,
                                                            multiplier);
        while (!finished(state)) {
            randomWalkStrategy.numberMovesToTake = numberRandomMoves.getValue();
            randomWalkStrategy.run(state, false);
            climbStrategy->run(state, false);
            if (quality(state) < bestQuality) {
                // smaller is better
                bestQuality = quality(state);
                numberRandomMoves.reset(baseValue, multiplier);
            } else {
                numberRandomMoves.increment();
                climbStrategy->allowedIterationsAtPeak.increment();
                if (climbStrategy->allowedIterationsAtPeak.getValue() / 100000) {
                    climbStrategy->allowedIterationsAtPeak.reset(1000,1.3);
                }
                if (numberRandomMoves.getValue() >
                    DEFAULT_PEAK_ITERATIONS) {
                    // reset
                    bestQuality = quality(state);
                    numberRandomMoves.reset(baseValue, multiplier);
                    //                    std::cout << "quality reset to " <<
                    //                    bestQuality << std::endl;
                }
            }
        }
    }
    void run(State& state, bool) {
        randomWalkStrategy.allowViolations = true;
        runImpl(state, [](State& state) { return state.model.getViolation(); },
                [](State& state) { return state.model.getViolation() == 0; });
        randomWalkStrategy.allowViolations = false;
        runImpl(state, [](State& state) { return state.model.getObjective(); },
                [](State&) { return false; });
    }
};
template <typename ClimbStrategy>
auto makeExplorationUsingRandomWalk(
    std::shared_ptr<ClimbStrategy> climbStrategy) {
    return std::make_shared<ExplorationUsingRandomWalk<ClimbStrategy>>(
        climbStrategy);
}

#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
