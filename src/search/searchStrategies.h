
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <deque>
#include "search/model.h"
#include "search/statsContainer.h"

u_int64_t ALLOWED_ITERATIONS_OF_INACTIVITY = 5000;
class HillClimbingExploiter {
    u_int64_t iterationsSpentAtPeak;

   public:
    HillClimbingExploiter(Model&) {}
    void initialise(const Model&, const StatsContainer&) {
        iterationsSpentAtPeak = 0;
    }
    bool newIteration(const Model&, const StatsContainer&) {
        return iterationsSpentAtPeak++ < ALLOWED_ITERATIONS_OF_INACTIVITY;
    }

    bool acceptSolution(const NeighbourhoodResult& result, StatsContainer&) {
        bool notOptimiseMode = result.model.optimiseMode == OptimiseMode::NONE;
        bool allowed = result.getDeltaViolation() < 0 ||
                       (result.getViolation() == 0 &&
                        (notOptimiseMode || result.getDeltaObjective() <= 0));
        bool strictlyBetter =
            result.getDeltaViolation() < 0 ||
            (result.getViolation() == 0 && result.getDeltaObjective() < 0);
        if (allowed && strictlyBetter) {
            iterationsSpentAtPeak = 0;
        }
        return allowed;
    }
};
static const int queueSize = 200;
class LateAcceptanceHillClimbingExploiter {
    u_int64_t iterationsSpentAtPeak;
    std::deque<Int> historyOfObjectives;
    std::deque<UInt> historyOfViolations;
    Int bestObjective;
UInt bestViolation;
   public:
    LateAcceptanceHillClimbingExploiter(Model&) {}
    void initialise(const Model& model, const StatsContainer&) {
        bestObjective = model.objective->view().value;
        bestViolation= model.csp->view().violation;
        historyOfViolations.clear();
        historyOfViolations.resize(queueSize,bestViolation);
        if (bestViolation == 0) {
            historyOfObjectives.clear();
            historyOfObjectives.resize(queueSize, bestObjective);
                    }
        iterationsSpentAtPeak = 0;
    }
    bool newIteration(const Model&, const StatsContainer&) {
        return iterationsSpentAtPeak++ < ALLOWED_ITERATIONS_OF_INACTIVITY;
    }

    bool acceptSolution(const NeighbourhoodResult& result, StatsContainer&) {
        bool notOptimiseMode = result.model.optimiseMode == OptimiseMode::NONE;
        assert(!notOptimiseMode);
        auto maxViolationAllowed = std::max(result.statsMarkPoint.lastViolation,historyOfViolations.front());
        if (result.getViolation() < bestViolation) {
            iterationsSpentAtPeak = 0;
            if (result.getViolation() == 0) {
                bestObjective = result.getObjective();
                historyOfObjectives.clear();
                historyOfObjectives.resize(queueSize, bestObjective);
            }
        }
        historyOfViolations.pop_front();
        if (result.getViolation() <= maxViolationAllowed) {
            historyOfViolations.push_back(result.getViolation());
            if (result.getDeltaViolation() < 0) {
                return true;
            }
        } else {
            UInt currentViolation = historyOfViolations.back();
            historyOfViolations.push_back(currentViolation);
            return false;
        }

        if (result.getViolation() == 0) {
            Int objectiveLimit =
                (result.model.optimiseMode == OptimiseMode::MAXIMISE)
                    ? std::min(result.statsMarkPoint.lastObjective,
                               historyOfObjectives.front())
                    : std::max(result.statsMarkPoint.lastObjective,
                               historyOfObjectives.front());
            bool acceptNewObjective =
                calcDeltaObjective(result.model.optimiseMode,
                                   result.getObjective(), objectiveLimit) <= 0;
            bool strictlyBetter =
                calcDeltaObjective(result.model.optimiseMode,
                                   result.getObjective(), bestObjective) < 0;
            if (strictlyBetter) {
                bestObjective = result.getObjective();
                iterationsSpentAtPeak = 0;
            }
            historyOfObjectives.pop_front();
            if (acceptNewObjective) {
                historyOfObjectives.push_back(result.getObjective());
            } else {
                Int currentObjective = historyOfObjectives.back();
                historyOfObjectives.push_back(currentObjective);
            }
            return acceptNewObjective;
        }
        return result.getViolation() <= maxViolationAllowed;
    }
};

enum class SearchMode { EXPLOIT, EXPLORE };
template <typename Exploiter>
struct ExplorationUsingViolationBackOff {
    SearchMode mode = SearchMode::EXPLOIT;
    Exploiter exploiter;
    u_int64_t violationBackOff;
    u_int64_t iterationsSinceLastImprove;
    UInt lastViolation;
    Int lastObjective;
    bool betterObjectiveFound;
    ExplorationUsingViolationBackOff(Model& model) : exploiter(model) {}
    void initialise(const Model& model, const StatsContainer& stats) {
        mode = SearchMode::EXPLOIT;
        exploiter.initialise(model, stats);
        violationBackOff = 1;
    }

    inline void increaseViolationBackOff() {
        violationBackOff *= 2;
        if (violationBackOff >= ((u_int64_t)1) << 32) {
            violationBackOff = 1;
//            std::cout << "Violation back off reset\n";
        }
//        std::cout << "Violation back off set to " << violationBackOff
//                  << std::endl;
    }

    bool newIteration(const Model& model, const StatsContainer& stats) {
        if (mode == SearchMode::EXPLOIT) {
            if (!exploiter.newIteration(model, stats)) {
                violationBackOff = 1;
                iterationsSinceLastImprove = 0;
                lastObjective = stats.bestObjective;
                lastViolation = stats.bestViolation;
                betterObjectiveFound = false;
//                std::cout << "found=" << betterObjectiveFound
//                          << "\niteration=" << iterationsSinceLastImprove
//                          << std::endl;
                mode = SearchMode::EXPLORE;
//                std::cout << "Exploring\n";
            }
        } else {
            ++iterationsSinceLastImprove;
            if (!betterObjectiveFound &&
                iterationsSinceLastImprove % ALLOWED_ITERATIONS_OF_INACTIVITY ==
                    0) {
                increaseViolationBackOff();
            } else if (betterObjectiveFound &&
                       iterationsSinceLastImprove %
                               ALLOWED_ITERATIONS_OF_INACTIVITY ==
                           0) {
                increaseViolationBackOff();
                betterObjectiveFound = false;
//                std::cout << "found=" << betterObjectiveFound
//                          << "\niteration=" << iterationsSinceLastImprove
//                          << std::endl;
            }
        }
        return true;
    }
    bool acceptSolution(const NeighbourhoodResult& result,
                        StatsContainer& stats) {
        if (mode == SearchMode::EXPLOIT) {
            return exploiter.acceptSolution(result, stats);
        } else {
            auto deltaObj =
                calcDeltaObjective(result.model.optimiseMode,
                                   result.getObjective(), lastObjective);
            auto deltaViolation = result.getViolation() - lastViolation;

            if (!betterObjectiveFound) {
                betterObjectiveFound =
                    deltaObj < 0 &&
                    ((u_int64_t)deltaViolation) <= violationBackOff;
                if (betterObjectiveFound) {
                    iterationsSinceLastImprove = 0;
//                    std::cout << "found=" << betterObjectiveFound
//                              << "\niteration=" << iterationsSinceLastImprove
//                              << std::endl;
                }
                return betterObjectiveFound;
            } else {
                bool allowed = deltaObj < 0 && result.getDeltaViolation() <= 0;
                // check if strictly better
                if (allowed && result.getDeltaViolation() < 0) {
                    iterationsSinceLastImprove = 0;
                }
                if (allowed && result.getViolation() == 0) {
                    mode = SearchMode::EXPLOIT;
                    exploiter.initialise(result.model, stats);
                }
                return allowed;
            }
        }
    }
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
