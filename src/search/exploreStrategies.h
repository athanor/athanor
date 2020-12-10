
#ifndef SRC_SEARCH_EXPLORESTRATEGIES_H_
#define SRC_SEARCH_EXPLORESTRATEGIES_H_
#include <cmath>
#include <deque>
#include "search/endOfSearchException.h"
#include "search/improveStrategies.h"
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/solver.h"
#include "search/statsContainer.h"

//#define EXPLORE_LOG 1
#ifdef EXPLORE_LOG
#define log_explore(x) \
    std::cout << state.stats.getRealTime() << " " << x << std::endl
#else
#define log_explore(x)  //
#endif

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
                nhSelector->nextNeighbourhood(
                    state, SearchMode::LOOKING_FOR_VIO_IMPROVEMENT),
                [&](const auto& result) {
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
    static const int INCREASE_LIMIT = 10;
    RandomNeighbourhood exploreStrategy = RandomNeighbourhood();
    static const int baseValue = 10;
    static constexpr double multiplier = 1.2;
    ExponentialIncrementer<UInt> violationBackOff =
        ExponentialIncrementer<UInt>(baseValue, multiplier);

   public:
    ExplorationUsingViolationBackOff(
        std::shared_ptr<SearchStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}

    inline void resetExploreSize() {
        violationBackOff.reset(baseValue, multiplier);
    }

    inline void increaseExploreSize() { violationBackOff.increment(); }

    bool explore(State& state, const Objective& objToBeat) {
        UInt64 iterationsWithoutChange = 0;
        while (objToBeat <= state.model.getObjective()) {
            bool allowed = false;
            state.runNeighbourhood(
                exploreStrategy.nextNeighbourhood(
                    state, SearchMode::LOOKING_FOR_RAW_OBJ_IMPROVEMENT),
                [&](const auto& result) {
                    if (!result.foundAssignment) {
                        return false;
                    }
                    allowed = result.model.getViolation() <=
                                  violationBackOff.getValue() &&
                              result.model.getObjective() <= objToBeat;
                    return allowed;
                });
            if (!(state.model.getObjective() < objToBeat)) {
                ++iterationsWithoutChange;
                if (iterationsWithoutChange % improveStratPeakIterations == 0) {
                    return false;
                }
            }
        }
        iterationsWithoutChange = 0;
        while (state.model.getViolation() > 0) {
            bool allowed = false;
            state.runNeighbourhood(
                exploreStrategy.nextNeighbourhood(
                    state, SearchMode::LOOKING_FOR_VIO_IMPROVEMENT),
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
                if (iterationsWithoutChange % improveStratPeakIterations == 0) {
                    return false;
                }
            }
        }
        return true;
    }

    void climbTo0Violation(State& state) {
        while (state.model.getViolation() > 0) {
            climbStrategy->run(state, false);
        }
    }

    void run(State& state, bool) {
        bool explorationSupported =
            state.model.optimiseMode != OptimiseMode::NONE;
        if (!explorationSupported) {
            std::cout << "[warning]: This exploration strategy "
                         "does not work "
                         "with satisfaction "
                         "problems.  Will not enter explore "
                         "mode using this "
                         "strategy.\n";
        }

        climbTo0Violation(state);

        Objective objToBeat = state.model.getObjective();
        int numberIncreases = 0;
        while (true) {
            bool foundBetter = explore(state, objToBeat);
            if (foundBetter) {
                climbStrategy->run(state, false);
            }
            if (state.model.getViolation() == 0 &&
                state.model.getObjective() < objToBeat) {
                objToBeat = state.model.getObjective();
                resetExploreSize();
                numberIncreases = 0;
            } else if (numberIncreases < INCREASE_LIMIT) {
                increaseExploreSize();
                numberIncreases += 1;
            } else {
                climbTo0Violation(state);
                objToBeat = state.model.getObjective();
                resetExploreSize();
                numberIncreases = 0;
            }
        }
    }
};
class ExplorationUsingRandomWalk : public SearchStrategy {
    static const int INCREASE_LIMIT = 15;
    static constexpr double baseValue = 10;
    static constexpr double multiplier = 1.3;
    std::shared_ptr<SearchStrategy> climbStrategy;
    RandomWalk randomWalkStrategy = RandomWalk(true);
    ExponentialIncrementer<UInt64> numberRandomMoves =
        ExponentialIncrementer<UInt64>(baseValue, multiplier);

   public:
    ExplorationUsingRandomWalk(std::shared_ptr<SearchStrategy> climbStrategy)
        : climbStrategy(std::move(climbStrategy)) {}

    void explore(State& state) {
        randomWalkStrategy.numberMovesToTake = numberRandomMoves.getValue();
        randomWalkStrategy.run(state, false);
    }

    void resetExploreSize() { numberRandomMoves.reset(baseValue, multiplier); }
    inline void increaseExploreSize() { numberRandomMoves.increment(); }
    template <typename FinishedFunc>
    void runImpl(State& state, FinishedFunc finished) {
        int numberIncreases = 0;
        auto objToBeat = state.model.getObjective();
        auto vioToBeat = state.model.getViolation();
        while (!finished(state)) {
            climbStrategy->run(state, false);
            if (state.model.getViolation() < vioToBeat ||
                (vioToBeat == 0 && state.model.getViolation() == 0 &&
                 state.model.getObjective() < objToBeat)) {
                vioToBeat = state.model.getViolation();
                objToBeat = state.model.getObjective();
                resetExploreSize();
                numberIncreases = 0;
                continue;
            }
            explore(state);

            if (numberIncreases < INCREASE_LIMIT) {
                increaseExploreSize();
                numberIncreases += 1;
            } else {
                resetExploreSize();
                vioToBeat = state.model.getViolation();
                objToBeat = state.model.getObjective();
                numberIncreases = 0;
            }
        }
    }

    void run(State& state, bool) {
        runImpl(state,
                [](State& state) { return state.model.getViolation() == 0; });
        resetExploreSize();
        runImpl(state, [](State&) { return false; });
    }
};

class ExplorationUsingAuto : public SearchStrategy,
                             public UcbSelector<ExplorationUsingAuto> {
    static const int INCREASE_LIMIT = 20;
    std::shared_ptr<SearchStrategy> climbStrategy;
    std::array<UInt, 2> numberActivations = {0, 0};
    std::array<UInt, 2> rewards = {0, 0};

   public:
    ExplorationUsingAuto(std::shared_ptr<SearchStrategy> climbStrategy)
        : UcbSelector<ExplorationUsingAuto>(1.2),
          climbStrategy(std::move(climbStrategy)) {}

    inline double reward(size_t i) { return rewards[i]; }
    inline double individualCost(size_t i) { return numberActivations[i]; }

    inline double totalCost() {
        return numberActivations[0] + numberActivations[1];
    }
    inline bool wasActivated(size_t i) { return numberActivations[i] > 0; }
    inline size_t numberOptions() { return 2; }

    void climbTo0Violation(State& state,
                           ExplorationUsingRandomWalk& rwExplorer) {
        if (state.model.getViolation() == 0) {
            return;
        }
        climbStrategy->run(state, false);
        rwExplorer.runImpl(state, [](State& state) {
            return state.model.getViolation() == 0;
        });
        rwExplorer.resetExploreSize();
    }
    void run(State& state, bool) {
        ExplorationUsingRandomWalk rwExplorer(climbStrategy);
        ExplorationUsingViolationBackOff vbExplorer(climbStrategy);
        climbTo0Violation(state, rwExplorer);
        auto bestObj = state.model.getObjective();
        log_explore("start at " << bestObj);
        int numberIncreases = 0;
        while (true) {
            int chosenExploreStrat = next();
            log_explore("running " << chosenExploreStrat);
            bool climb = true;
            if (chosenExploreStrat == 0) {
                rwExplorer.explore(state);
            } else if (chosenExploreStrat == 1) {
                climb = vbExplorer.explore(state, bestObj);
            } else {
                myCerr << "Unrecognised chosenExploreStrat, internal "
                          "error, exiting...\n";
                signalEndOfSearch();
            }
            if (climb) {
                log_explore("Climbing");
                climbStrategy->run(state, false);
            }
            numberActivations[chosenExploreStrat] += 1;
            if (state.model.getViolation() == 0 &&
                state.model.getObjective() < bestObj) {
                rewards[chosenExploreStrat] += 1;
                vbExplorer.resetExploreSize();
                rwExplorer.resetExploreSize();
                bestObj = state.model.getObjective();
                log_explore("Accepting new obj " << bestObj);
            } else if (numberIncreases < INCREASE_LIMIT) {
                log_explore("rejecting");
                vbExplorer.increaseExploreSize();
                rwExplorer.increaseExploreSize();
                numberIncreases += 1;
            } else {
                vbExplorer.resetExploreSize();
                rwExplorer.resetExploreSize();
                climbTo0Violation(state, rwExplorer);
                numberIncreases = 0;
                bestObj = state.model.getObjective();
                log_explore("Accepting new obj " << bestObj);
            }
        }
    }

    inline void printAdditionalStats(std::ostream& os) final {
        os << "strat,activations,improvements\n";
        os << "rw," << numberActivations[0] << "," << rewards[0] << std::endl;
        os << "vb," << numberActivations[1] << "," << rewards[1] << std::endl;
    }
};

#endif /* SRC_SEARCH_EXPLORESTRATEGIES_H_ */
