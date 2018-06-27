
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
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
        bool allowed = (result.getViolation() > 0 && result.getDeltaViolation() <= 0) ||
               (result.getViolation() == 0 && result.getDeltaObjective() <= 0);
        if (allowed) {
            iterationsSpentAtPeak = 0;
        }
        return allowed;
    }
};

enum class SearchMode { EXPLOIT, EXPLORE };
template <typename Exploiter>
struct ExplorationUsingViolationBackOff {
    SearchMode mode = SearchMode::EXPLOIT;
    Exploiter exploiter;
    u_int64_t violationBackOff;
    u_int64_t iterationsSinceLastImprove;
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
            std::cout << "Violation back off reset\n";
        }
        std::cout << "Violation back off set to " << violationBackOff
                  << std::endl;

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
            ++iterationsSinceLastImprove;
            if (!betterObjectiveFound &&
                iterationsSinceLastImprove % ALLOWED_ITERATIONS_OF_INACTIVITY == 0) {
                increaseViolationBackOff();
            } else if (betterObjectiveFound && iterationsSinceLastImprove % ALLOWED_ITERATIONS_OF_INACTIVITY*5 == 0){
                increaseViolationBackOff();
                betterObjectiveFound = false;

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
                    ((u_int64_t)result.getDeltaViolation()) <= violationBackOff;
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
