
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <cmath>
#include <deque>
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/solver.h"
#include "search/statsContainer.h"

extern UInt64 improveStratPeakIterations;

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
        if (value > ((UInt64)1) << 32) {
            value = 1;
        }
        value *= exponent;
    }
};

template <typename Strategy>
class HillClimbing {
    std::shared_ptr<Strategy> strategy;
    const UInt64 allowedIterationsAtPeak = improveStratPeakIterations;

   public:
    HillClimbing(std::shared_ptr<Strategy> strategy)
        : strategy(std::move(strategy)) {}

    void reset() {}
    void increaseTerminationLimit() {}

    void resetTerminationLimit() {}

    void run(State& state, bool isOuterMostStrategy) {
        UInt64 iterationsAtPeak = 0;
        while (true) {
            bool allowed = false, strictImprovement = false;
            strategy->run(state, [&](const auto& result) {
                if (result.foundAssignment) {
                    if (result.statsMarkPoint.lastViolation != 0) {
                        allowed = result.getDeltaViolation() <= 0;
                        strictImprovement = result.getDeltaViolation() < 0;
                    } else {
                        allowed = result.model.getViolation() == 0 &&
                                  result.objectiveBetterOrEqual();
                        strictImprovement =
                            allowed && result.objectiveStrictlyBetter();
                    }
                }
                return allowed;
            });
            if (isOuterMostStrategy) {
                continue;
            }
            if (strictImprovement) {
                iterationsAtPeak = 0;
            } else {
                ++iterationsAtPeak;
                if (iterationsAtPeak > allowedIterationsAtPeak) {
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

template <typename Strategy>
class LateAcceptanceHillClimbing {
    std::shared_ptr<Strategy> strategy;
    const UInt64 allowedIterationsAtPeak = improveStratPeakIterations;
    std::deque<Objective> objHistory;
    std::deque<UInt> vioHistory;
    size_t queueSize;

   public:
    LateAcceptanceHillClimbing(std::shared_ptr<Strategy> strategy,
                               size_t queueSize)
        : strategy(std::move(strategy)), queueSize(queueSize) {}

    void reset() {}
    void increaseTerminationLimit() {}

    void resetTerminationLimit() {}
    template <typename T>
    void addToQueue(std::deque<T>& queue, T value) {
        static int i = 0;
        if (queue.size() == queueSize) {
            queue.pop_front();
        }
        queue.emplace_back(value);
    }
    void run(State& state, bool isOuterMostStrategy) {
        objHistory.clear();
        vioHistory.clear();
        UInt64 iterationsAtPeak = 0;
        if (state.model.getViolation() > 0) {
            vioHistory.emplace_back(state.model.getViolation());
        } else {
            objHistory.emplace_back(state.model.getObjective());
        }
        Objective bestObjective = state.model.getObjective();
        UInt bestViolation = state.model.getViolation();
        while (true) {
            bool allowed = false, improvesOnBest = false;
            strategy->run(state, [&](const auto& result) {
                if (result.foundAssignment) {
                    if (result.statsMarkPoint.lastViolation != 0) {
                        allowed =
                            result.model.getViolation() <= vioHistory.front() ||
                            result.model.getViolation() <= vioHistory.back();
                        improvesOnBest =
                            result.model.getViolation() < bestViolation;
                        if (allowed) {
                            addToQueue(vioHistory, state.model.getViolation());
                        }

                        if (improvesOnBest) {
                            bestViolation = result.model.getViolation();
                        }
                        if (result.statsMarkPoint.lastViolation != 0 &&
                            result.model.getViolation() == 0) {
                            objHistory = {state.model.getObjective()};
                        }
                    } else if (result.model.getViolation() == 0) {
                        // last violation was 0, current violation is 0, check
                        // objective:
                        allowed =
                            result.model.getObjective() <= objHistory.front() ||
                            result.model.getObjective() <= objHistory.back();
                        improvesOnBest =
                            result.model.getObjective() < bestObjective;
                        if (allowed) {
                            addToQueue(objHistory, state.model.getObjective());
                        }

                        if (improvesOnBest) {
                            bestObjective = result.model.getObjective();
                        }
                    }
                }
                return allowed;
            });

            if (isOuterMostStrategy) {
                continue;
            }
            if (improvesOnBest) {
                iterationsAtPeak = 0;
            } else {
                ++iterationsAtPeak;
                if (iterationsAtPeak > allowedIterationsAtPeak) {
                    break;
                }
            }
        }
    }
};

template <typename Strategy>
auto makeLateAcceptanceHillClimbing(std::shared_ptr<Strategy> strategy,
                                    size_t queueSize) {
    return std::make_shared<LateAcceptanceHillClimbing<Strategy>>(
        std::move(strategy), queueSize);
}

class RandomWalk {
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
            nhSelector->run(state, [&](const auto& result) {
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
            exploreStrategy->run(state, [&](const auto& result) {
                stepTaken = result.foundAssignment;
                return stepTaken;
            });
        }
    }

    bool repair(State& state, const Objective& objToBeat) {
        UInt64 iterationsWithoutChange = 0;
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
    static constexpr UInt64 MAX_NUMBER_RANDOM_MOVES = 500;
    static constexpr double baseValue = 10;
    static constexpr double multiplier = 1.3;
    std::shared_ptr<ClimbStrategy> climbStrategy;
    RandomWalk randomWalkStrategy = RandomWalk(false);

   public:
    ExplorationUsingRandomWalk(std::shared_ptr<ClimbStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}
    template <typename Func1, typename Func2>
    void runImpl(State& state, Func1&& quality, Func2&& finished) {
        climbStrategy->reset();
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
                climbStrategy->increaseTerminationLimit();
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
template <typename ClimbStrategy>
auto makeExplorationUsingRandomWalk(
    std::shared_ptr<ClimbStrategy> climbStrategy) {
    return std::make_shared<ExplorationUsingRandomWalk<ClimbStrategy>>(
        climbStrategy);
}

template <typename Strategy>
class TestClimber {
    std::shared_ptr<Strategy> strategy;

   public:
    TestClimber(std::shared_ptr<Strategy> strategy)
        : strategy(std::move(strategy)) {}

    void reset() {}
    void increaseTerminationLimit() { todoImpl(); }
    void resetTerminationLimit() { todoImpl(); }

    void run(State& state, bool isOuterMostStrategy) {
        ignoreUnused(state, isOuterMostStrategy);
        todoImpl();
    }
};

template <typename Strategy>
auto makeTestClimb(std::shared_ptr<Strategy> strategy) {
    return std::make_shared<TestClimber<Strategy>>(std::move(strategy));
}

template <typename ClimbStrategy>
class TestExploration {
    static constexpr double baseValue = 10;
    static constexpr double multiplier = 1.3;
    std::shared_ptr<ClimbStrategy> climbStrategy;

   public:
    TestExploration(std::shared_ptr<ClimbStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}

    void run(State& state, bool) {
        ignoreUnused(state);
        todoImpl();
    }
};

template <typename ClimbStrategy>
auto makeTestExploration(std::shared_ptr<ClimbStrategy> climbStrategy) {
    return std::make_shared<TestExploration<ClimbStrategy>>(climbStrategy);
}

#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
