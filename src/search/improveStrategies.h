
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

   public:
    HillClimbing(std::shared_ptr<NeighbourhoodSelectionStrategy> selector,
                 std::shared_ptr<NeighbourhoodSearchStrategy> searcher)
        : selector(std::move(selector)), searcher(std::move(searcher)) {}

    void run(State& state, bool isOuterMostStrategy) {
        UInt64 iterationsAtPeak = 0;
        while (true) {
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
    std::shared_ptr<NeighbourhoodSelectionStrategy> strategy;
    const UInt64 allowedIterationsAtPeak = improveStratPeakIterations;
    std::deque<Objective> objHistory;
    std::deque<UInt> vioHistory;
    size_t queueSize;

   public:
    LateAcceptanceHillClimbing(
        std::shared_ptr<NeighbourhoodSelectionStrategy> strategy,
        size_t queueSize)
        : strategy(std::move(strategy)), queueSize(queueSize) {}

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
            bool allowed = false, improvesOnBest = false;
            state.runNeighbourhood(
                strategy->nextNeighbourhood(state), [&](const auto& result) {
                    if (result.foundAssignment) {
                        if (result.statsMarkPoint.lastViolation != 0) {
                            allowed = result.model.getViolation() <=
                                          vioHistory.front() ||
                                      result.model.getViolation() <=
                                          vioHistory.back();
                            improvesOnBest =
                                result.model.getViolation() < bestViolation;
                            if (allowed) {
                                addToQueue(vioHistory,
                                           state.model.getViolation());
                            }

                            if (improvesOnBest) {
                                bestViolation = result.model.getViolation();
                            }
                            if (result.statsMarkPoint.lastViolation != 0 &&
                                result.model.getViolation() == 0) {
                                objHistory = {state.model.getObjective()};
                            }
                        } else if (result.model.getViolation() == 0) {
                            // last violation was 0, current violation is 0,
                            // check objective:
                            allowed = result.model.getObjective() <=
                                          objHistory.front() ||
                                      result.model.getObjective() <=
                                          objHistory.back();
                            improvesOnBest =
                                result.model.getObjective() < bestObjective;
                            if (allowed) {
                                addToQueue(objHistory,
                                           state.model.getObjective());
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

#endif /* SRC_SEARCH_IMPROVESTRATEGIES_H_ */
