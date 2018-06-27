
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include "search/model.h"
#include "search/statsContainer.h"
class HillClimbingExploiter {
    int iterationsSpentAtPeak;

   public:
    HillClimbingExploiter(Model&) {}
    void initialise(const Model&, const StatsContainer&) {
        iterationsSpentAtPeak = 0;
    }
    bool newIteration(const Model&, const StatsContainer&) {
        return ++iterationsSpentAtPeak < 120000;
    }

    bool acceptSolution(const NeighbourhoodResult& result, StatsContainer&) {
        return result.getDeltaViolation() <= 0 ||
               (result.getViolation() == 0 && result.getDeltaObjective() <= 0);
    }
};

enum class SearchMode { EXPLOIT, EXPLORE };
template <typename Exploiter>
struct ExplorationUsingViolationBackOff {
    SearchMode mode = SearchMode::EXPLOIT;
    Exploiter exploiter;
    UInt violationBackOff;
    u_int64_t iterationsSinceLastImprove;
    Int lastObjective;
    bool betterObjectiveFound;
    ExplorationUsingViolationBackOff(Model& model) : exploiter(model) {}
    void initialise(const Model& model, const StatsContainer& stats) {
        mode = SearchMode::EXPLOIT;
        exploiter.initialise(model, stats);
        violationBackOff = 1;
    }

    bool newIteration(const Model& model, const StatsContainer& stats) {
        if (mode == SearchMode::EXPLOIT) {
            if (!exploiter.newIteration(model, stats)) {
                violationBackOff = 1;
                iterationsSinceLastImprove = 0;
                lastObjective = model.objective->view().value;
                betterObjectiveFound = false;
                mode = SearchMode::EXPLORE;
                std::cout << "Exploring\n";
            }
        } else {
            if (!betterObjectiveFound &&
                ++iterationsSinceLastImprove % 120000 == 0) {
                violationBackOff *= 2;
                std::cout << "Violation back off set to " << violationBackOff
                          << std::endl;
            }
        }
        return true;
    }
    bool acceptSolution(const NeighbourhoodResult& result,
                        StatsContainer& stats) {
        if (mode == SearchMode::EXPLOIT) {
            return exploiter.acceptSolution(result, stats);
        } else {
            if (!betterObjectiveFound) {
                betterObjectiveFound =
                    result.getDeltaObjective() < 0 &&
                    result.getDeltaViolation() <= violationBackOff;
                return betterObjectiveFound;
            } else {
                bool allowed = calcDeltaObjective(result.model.optimiseMode,
                                                  result.getObjective(),
                                                  lastObjective) < 0 &&
                               result.getDeltaViolation() <= 0;
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
