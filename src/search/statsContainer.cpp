#include "search/statsContainer.h"
#include "search/model.h"
Int NeighbourhoodResult::getDeltaViolation() const {
    return model.getViolation() - statsMarkPoint.lastViolation;
}
Int NeighbourhoodResult::getDeltaObjective() const {
    return model.getObjective() - statsMarkPoint.lastObjective;
}
Int NeighbourhoodResult::getDeltaDefinedness() const {
    bool currentDefined = model.objective->getViewIfDefined().hasValue();
    return currentDefined - statsMarkPoint.lastObjectiveDefined;
}

StatsContainer::StatsContainer(Model& model)
    : optimiseMode(model.optimiseMode),
      nhActivationCounts(model.neighbourhoods.size(), 0),
      nhMinorNodeCounts(model.neighbourhoods.size(), 0),
      nhTriggerEventCounts(model.neighbourhoods.size(), 0),
      nhTotalCpuTimes(model.neighbourhoods.size(), 0) {}
#include <iostream>
void StatsContainer::initialSolution(Model& model) {
    lastViolation = model.csp->view()->violation;
    lastObjectiveDefined = model.objective->getViewIfDefined().hasValue();
    if (lastObjectiveDefined) {
        lastObjective = model.getObjective();
    }
    bestViolation = lastViolation;
    bestObjective = lastObjective;
    checkForBestSolution(true, true, model);
}

void StatsContainer::reportResult(bool solutionAccepted,
                                  const NeighbourhoodResult& result) {
    ++numberIterations;
    ++nhActivationCounts[result.neighbourhoodIndex];
    nhMinorNodeCounts[result.neighbourhoodIndex] +=
        minorNodeCount - result.statsMarkPoint.minorNodeCount;
    nhTriggerEventCounts[result.neighbourhoodIndex] +=
        triggerEventCount - result.statsMarkPoint.triggerEventCount;
    nhTotalCpuTimes[result.neighbourhoodIndex] +=
        getCpuTime() - result.statsMarkPoint.cpuTime;
    if (!solutionAccepted) {
        return;
    }
    lastViolation = result.model.csp->view()->violation;
    lastObjectiveDefined =
        result.model.objective->getViewIfDefined().hasValue();
    if (lastObjectiveDefined) {
        lastObjective = result.model.getObjective();
    }
    bool vioImproved = lastViolation < bestViolation;
    bool objImproved = lastObjective < bestObjective;
    checkForBestSolution(vioImproved, objImproved, result.model);
}

void StatsContainer::checkForBestSolution(bool vioImproved, bool objImproved,
                                          Model& model) {
    // following is a bit messy for printing purposes
    if (vioImproved) {
        bestViolation = lastViolation;
    }
    if ((bestViolation == 0 && lastViolation == 0 && objImproved) ||
        (bestViolation != 0 && vioImproved)) {
        bestObjective = lastObjective;
    }
    if (vioImproved || (lastViolation == 0 && objImproved)) {
        std::cout << "\nNew solution:\n";
        std::cout << "Violation = " << lastViolation << std::endl;
        if (model.optimiseMode != OptimiseMode::NONE) {
            std::cout << "objective = "
                      << transposeObjective(optimiseMode, lastObjective)
                      << std::endl;
        }
        // for experiements
        if (lastViolation == 0) {
            std::cout << "Best solution: "
                      << transposeObjective(optimiseMode, bestObjective) << " "
                      << getCpuTime() << std::endl;
        }
    }

    if (vioImproved || (lastViolation == 0 && objImproved)) {
        printCurrentState(model);
    }
}
void StatsContainer::printCurrentState(Model& model) {
    std::cout << (*this) << "\nTrigger event count " << triggerEventCount
              << "\n\n";
    if (lastViolation == 0) {
        std::cout << "solution start\n";
        model.printVariables();
        std::cout << "solution end\n";
    }
    debug_code(if (debugLogAllowed) {
        debug_log("CSP state:");
        model.csp->dumpState(std::cout) << std::endl;
        if (model.optimiseMode != OptimiseMode::NONE) {
            debug_log("Objective state:");
            model.objective->dumpState(std::cout) << std::endl;
        }
    });
}

std::ostream& operator<<(std::ostream& os, StatsContainer& stats) {
    stats.endTimer();

    os << "Stats:\n";
    os << "Best violation: " << stats.bestViolation << std::endl;
    os << "Best objective: "
       << transposeObjective(stats.optimiseMode, stats.bestObjective)
       << std::endl;

    os << "Number iterations: " << stats.numberIterations << std::endl;
    os << "Minor node count: " << stats.minorNodeCount << std::endl;
    std::chrono::duration<double> timeTaken = stats.endTime - stats.startTime;
    os << "Wall time: " << timeTaken.count() << std::endl;
    auto cpuTime =
        (double)(stats.endCpuTime - stats.startCpuTime) / CLOCKS_PER_SEC;
    os << "CPU time: " << cpuTime;
    return os;
}
