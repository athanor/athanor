
#ifndef SRC_SEARCH_IMPROVESTRATEGIES_H_
#define SRC_SEARCH_IMPROVESTRATEGIES_H_
#include <cmath>
#include <deque>
#include "search/model.h"
#include "search/neighbourhoodSearchStrategies.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/searchStrategies.h"
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
class HillClimbing : public SearchStrategy {
    std::shared_ptr<NeighbourhoodSelectionStrategy> selector;
    std::shared_ptr<NeighbourhoodSearchStrategy> searcher;
    const UInt64 allowedIterationsAtPeak = improveStratPeakIterations;
    const UInt64 maxIterations;

   public:
    HillClimbing(std::shared_ptr<NeighbourhoodSelectionStrategy> selector,
                 std::shared_ptr<NeighbourhoodSearchStrategy> searcher,
                 UInt64 maxIterations = 0)
        : selector(std::move(selector)),
          searcher(std::move(searcher)),
          maxIterations(maxIterations) {}

    void run(State& state, bool isOuterMostStrategy) {
        UInt64 iterationsAtPeak = 0;
        UInt64 startNumberIterations = state.stats.numberIterations;
        while (maxIterations == 0 ||
               state.stats.numberIterations - startNumberIterations <
                   maxIterations) {
            bool allowed = false, strictImprovement = false;
            searcher->search(
                state, selector->nextNeighbourhood(state),
                [&](const auto& result) {
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

class LateAcceptanceHillClimbing : public SearchStrategy {
    std::shared_ptr<NeighbourhoodSelectionStrategy> selector;
    std::shared_ptr<NeighbourhoodSearchStrategy> searcher;
    const UInt64 allowedIterationsAtPeak = improveStratPeakIterations;
    std::deque<Objective> objHistory;
    std::deque<UInt> vioHistory;
    size_t queueSize;

   public:
    LateAcceptanceHillClimbing(
        std::shared_ptr<NeighbourhoodSelectionStrategy> selector,
        std::shared_ptr<NeighbourhoodSearchStrategy> searcher, size_t queueSize)
        : selector(std::move(selector)),
          searcher(std::move(searcher)),
          queueSize(queueSize) {}

    template <typename T>
    void addToQueue(std::deque<T>& queue, T value) {
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
            bool wasViolating = state.model.getViolation();
            searcher->search(
                state, selector->nextNeighbourhood(state),
                [&](const auto& result) {
                    bool allowed = false;
                    if (result.foundAssignment) {
                        if (result.statsMarkPoint.lastViolation != 0) {
                            allowed = result.model.getViolation() <=
                                          vioHistory.front() ||
                                      result.model.getViolation() <=
                                          vioHistory.back();
                        } else if (result.model.getViolation() == 0) {
                            // last violation was 0, current violation is 0,
                            // check objective:
                            allowed = result.model.getObjective() <=
                                          objHistory.front() ||
                                      result.model.getObjective() <=
                                          objHistory.back();
                        }
                    }
                    return allowed;
                });
            bool isViolating = state.model.getViolation() > 0;
            bool improvesOnBest = false;
            if (wasViolating && isViolating) {
                UInt violation = state.model.getViolation();
                addToQueue(vioHistory, violation);
                improvesOnBest = violation < bestViolation;
            } else if (wasViolating && !isViolating) {
                vioHistory.clear();
                objHistory.clear();
                addToQueue(objHistory, state.model.getObjective());
                improvesOnBest = true;
            } else if (!wasViolating && !isViolating) {
                auto obj = state.model.getObjective();
                addToQueue(objHistory, obj);
                improvesOnBest = obj < bestObjective;
            }
            if (improvesOnBest) {
                bestViolation = state.model.getViolation();
                bestObjective = state.model.getObjective();
                iterationsAtPeak = 0;
            } else {
                ++iterationsAtPeak;
                if (!isOuterMostStrategy &&
                    iterationsAtPeak > allowedIterationsAtPeak) {
                    break;
                }
            }
        }
    }
};

class HillClimbingWithViolations : public SearchStrategy {
    std::shared_ptr<NeighbourhoodSelectionStrategy> selector;
    std::shared_ptr<NeighbourhoodSearchStrategy> searcher;

    static const int baseValue = 20;
    static constexpr double multiplier = 1.2;
    static const int INCREASE_LIMIT = 10;
    const UInt64 allowedIterationsAtPeak = improveStratPeakIterations;
    const UInt64 maxIterations;
    ExponentialIncrementer<UInt> violationBackOff =
        ExponentialIncrementer<UInt>(baseValue, multiplier);

   public:
    HillClimbingWithViolations(
        std::shared_ptr<NeighbourhoodSelectionStrategy> selector,
        std::shared_ptr<NeighbourhoodSearchStrategy> searcher,
        UInt64 maxIterations = 0)
        : selector(std::move(selector)),
          searcher(std::move(searcher)),
          maxIterations(maxIterations) {}
    inline void resetExploreSize() {
        violationBackOff.reset(baseValue, multiplier);
    }

    inline void increaseExploreSize() { violationBackOff.increment(); }

    bool hasResource(State& state, UInt64 startNumberIterations) {
        return maxIterations == 0 ||
               state.stats.numberIterations - startNumberIterations <=
                   maxIterations;
    }
    void explore(State& state, const Objective& objToBeat,
                 UInt64 startNumberIterations) {
        UInt64 iterationsWithoutChange = 0;
        while (hasResource(state, startNumberIterations) &&
               objToBeat <= state.model.getObjective()) {
            bool allowed = false;
            searcher->search(state, selector->nextNeighbourhood(state),
                             [&](const auto& result) {
                                 if (!result.foundAssignment) {
                                     return false;
                                 }
                                 allowed =
                                     result.model.getViolation() <=
                                         violationBackOff.getValue() &&
                                     result.model.getObjective() <= objToBeat;
                                 return allowed;
                             });
            if (!(state.model.getObjective() < objToBeat)) {
                ++iterationsWithoutChange;
                if (iterationsWithoutChange >
                    allowedIterationsAtPeak / INCREASE_LIMIT) {
                    return;
                }
            }
        }
        iterationsWithoutChange = 0;
        //        std::cout << "found obj " << state.model.getObjective() << ",
        //        trying to remove violation=" << state.model.getViolation() <<
        //        "\n";
        while (hasResource(state, startNumberIterations) &&
               state.model.getViolation() > 0) {
            bool allowed = false;
            searcher->search(state, selector->nextNeighbourhood(state),
                             [&](const auto& result) {
                                 if (!result.foundAssignment) {
                                     return false;
                                 }
                                 allowed =
                                     result.getDeltaViolation() <= 0 &&
                                     result.model.getObjective() < objToBeat;
                                 return allowed;
                             });
            if (!allowed) {
                ++iterationsWithoutChange;
                if (iterationsWithoutChange >
                    allowedIterationsAtPeak / INCREASE_LIMIT) {
                    return;
                }
            }
        }
        return;
    }

    bool climbTo0Violation(State& state) {
        UInt64 iterationsAtPeak = 0;
        while (state.model.getViolation() > 0) {
            bool allowed = false, strictImprovement = false;
            searcher->search(state, selector->nextNeighbourhood(state),
                             [&](const auto& result) {
                                 if (result.foundAssignment) {
                                     allowed = result.getDeltaViolation() <= 0;
                                     strictImprovement =
                                         result.getDeltaViolation() < 0;
                                 }
                                 return allowed;
                             });
            if (strictImprovement) {
                iterationsAtPeak = 0;
            } else {
                ++iterationsAtPeak;
                if (iterationsAtPeak > allowedIterationsAtPeak) {
                    return false;
                }
            }
        }
        return true;
    }

    void run(State& state, bool isOuterMostStrategy) {
        while (true) {
            bool is0violations = climbTo0Violation(state);
            if (is0violations) {
                break;
            } else if (isOuterMostStrategy) {
                continue;
            } else {
                return;
            }
        }

        Objective objToBeat = state.model.getObjective();
        int numberIncreases = 0;
        UInt64 startNumberIterations = state.stats.numberIterations;
        while (hasResource(state, startNumberIterations)) {
            //            std::cout << "exploring, violation=" <<
            //            state.model.getViolation() << ", numberIterations=" <<
            //            state.stats.numberIterations << "\n";
            explore(state, objToBeat, startNumberIterations);
            if (state.model.getViolation() == 0 &&
                state.model.getObjective() < objToBeat) {
                objToBeat = state.model.getObjective();
                resetExploreSize();
                numberIncreases = 0;
                //                std::cout << "found better violation=" <<
                //                state.model.getViolation() << "\n";
            } else if (numberIncreases < INCREASE_LIMIT) {
                increaseExploreSize();
                numberIncreases += 1;
                //                std::cout << "did not find better\n";
            } else {
                //                std::cout << "giving up\n";
                objToBeat = state.model.getObjective();
                resetExploreSize();
                numberIncreases = 0;
                return;
            }
        }
    }
};

class MetaHillClimbing : public SearchStrategy,
                         public UcbSelector<MetaHillClimbing> {
    std::shared_ptr<NeighbourhoodSelectionStrategy> selector;
    std::shared_ptr<NeighbourhoodSearchStrategy> searcher;

    HillClimbing search1;
    HillClimbingWithViolations search2;
    const UInt64 allowedIterationsAtPeak = improveStratPeakIterations;
    UInt64 search1Iterations = 0;
    UInt64 search2Iterations = 0;
    UInt64 search1Solutions = 0;
    UInt64 search2Solutions = 0;
    UInt64 search1Activations = 0;
    UInt64 search2Activations = 1;  // marking this as activated as forcing UCB
                                    // to choose other strategy first.

   public:
    MetaHillClimbing(std::shared_ptr<NeighbourhoodSelectionStrategy> selector,
                     std::shared_ptr<NeighbourhoodSearchStrategy> searcher)
        : UcbSelector<MetaHillClimbing>(1),
          selector(selector),
          searcher(searcher),
          search1(selector, searcher, improveStratPeakIterations),
          search2(selector, searcher, improveStratPeakIterations) {}

    inline double individualCost(size_t i) {
        return (i == 0) ? search1Activations : search2Activations;
    }
    inline double totalCost() {
        return search1Activations + search2Activations;
    }
    inline size_t numberOptions() { return 2; }
    inline bool wasActivated(size_t i) {
        return (i == 0) ? search1Activations != 0 : search2Activations != 0;
    }
    inline double reward(size_t i) {
        return (i == 0) ? search1Solutions : search2Solutions;
    }

    bool climbTo0Violation(State& state) {
        UInt64 iterationsAtPeak = 0;
        while (state.model.getViolation() > 0) {
            bool allowed = false, strictImprovement = false;
            searcher->search(state, selector->nextNeighbourhood(state),
                             [&](const auto& result) {
                                 if (result.foundAssignment) {
                                     allowed = result.getDeltaViolation() <= 0;
                                     strictImprovement =
                                         result.getDeltaViolation() < 0;
                                 }
                                 return allowed;
                             });
            if (strictImprovement) {
                iterationsAtPeak = 0;
            } else {
                ++iterationsAtPeak;
                if (iterationsAtPeak > allowedIterationsAtPeak) {
                    return false;
                }
            }
        }
        return true;
    }

    void run(State& state, bool isOuterMostStrategy) {
        while (true) {
            bool is0violations = climbTo0Violation(state);
            if (is0violations) {
                break;
            } else if (isOuterMostStrategy) {
                continue;
            } else {
                return;
            }
        }
        do {
            UInt64 startNumberSolutions =
                state.stats.numberBetterFeasibleSolutionsFound;
            UInt64 startNumberIterations = state.stats.numberIterations;
            auto choice = next();
            bool endOfSearch = false;
            try {
                if (choice == 0) {
                    search1.run(state, false);
                } else {
                    search2.run(state, false);
                }
            } catch (EndOfSearchException&) {
                endOfSearch = true;
            }
            UInt64 numberAdditionalSolutions =
                state.stats.numberBetterFeasibleSolutionsFound -
                startNumberSolutions;
            if (choice == 0) {
                search1Iterations +=
                    state.stats.numberIterations - startNumberIterations;
                search1Solutions += numberAdditionalSolutions;
                search1Activations += 1;
            } else {
                search2Iterations +=
                    state.stats.numberIterations - startNumberIterations;
                search2Solutions += numberAdditionalSolutions;
                search2Activations += 1;
            }
            if (endOfSearch) {
                signalEndOfSearch();
            }
        } while (isOuterMostStrategy);
    }
    inline void printAdditionalStats(std::ostream& os) final {
        os << "strat,activations,solutions,iterations\n";
        os << "climbing," << search1Activations << "," << search1Solutions
           << "," << search1Iterations << std::endl;
        os << "climbing with violations," << search2Activations << ","
           << search2Solutions << "," << search2Iterations << std::endl;
    }
};
#endif /* SRC_SEARCH_IMPROVESTRATEGIES_H_ */
