#include "search/statsContainer.h"
#include <iostream>
#include "search/model.h"

using namespace std;

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

ostream& operator<<(ostream& os, const NeighbourhoodStats& stats) {
    static const std::string indent = "    ";

    os << "Neighbourhood: " << stats.name << endl;
    os << indent << "value,total,average\n";
    os << indent << "Number activations," << stats.numberActivations << ","
       << ((stats.numberActivations == 0) ? 0 : 1) << endl;
    os << indent << "Number objective improvements,"
       << stats.numberObjImprovements << ","
       << stats.getAverage(stats.numberObjImprovements) << endl;
    os << indent << "Number violation improvements,"
       << stats.numberVioImprovmenents << ","
       << stats.getAverage(stats.numberVioImprovmenents) << endl;
    os << indent << "Number minor nodes," << stats.minorNodeCount << ","
       << stats.getAverage(stats.minorNodeCount) << endl;
    os << indent << "Number trigger events," << stats.triggerEventCount << ","
       << stats.getAverage(stats.triggerEventCount) << endl;
    os << indent << "Time," << stats.totalCpuTime << ","
       << stats.getAverage(stats.totalCpuTime);
    return os;
}

StatsContainer::StatsContainer(Model& model)
    : optimiseMode(model.optimiseMode) {
    for (auto& neighbourhood : model.neighbourhoods) {
        neighbourhoodStats.emplace_back(neighbourhood.name);
    }
}
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
    if (result.statsMarkPoint.lastViolation > 0) {
        ++numberVioIterations;
    }
    if (result.statsMarkPoint.lastViolation > 0) {
        vioMinorNodeCount +=
            minorNodeCount - result.statsMarkPoint.minorNodeCount;
    }
    ++neighbourhoodStats[result.neighbourhoodIndex].numberActivations;
    if (result.statsMarkPoint.lastViolation > 0) {
        ++neighbourhoodStats[result.neighbourhoodIndex].numberVioActivations;
    }
    neighbourhoodStats[result.neighbourhoodIndex].minorNodeCount +=
        minorNodeCount - result.statsMarkPoint.minorNodeCount;
    if (result.statsMarkPoint.lastViolation > 0) {
        neighbourhoodStats[result.neighbourhoodIndex].vioMinorNodeCount +=
            minorNodeCount - result.statsMarkPoint.minorNodeCount;
    }
    neighbourhoodStats[result.neighbourhoodIndex].triggerEventCount +=
        triggerEventCount - result.statsMarkPoint.triggerEventCount;
    if (result.statsMarkPoint.lastViolation > 0) {
        neighbourhoodStats[result.neighbourhoodIndex].vioTriggerEventCount +=
            triggerEventCount - result.statsMarkPoint.triggerEventCount;
    }
    double time = getCpuTime() - result.statsMarkPoint.cpuTime;
    neighbourhoodStats[result.neighbourhoodIndex].totalCpuTime += time;
    if (result.statsMarkPoint.lastViolation > 0) {
        neighbourhoodStats[result.neighbourhoodIndex].vioTotalCpuTime += time;
    }
    neighbourhoodStats[result.neighbourhoodIndex].numberObjImprovements +=
        (result.getDeltaObjective() < 0);
    neighbourhoodStats[result.neighbourhoodIndex].numberVioImprovmenents +=
        (result.getDeltaViolation() < 0);
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
        cout << "\nNew solution:\n";
        cout << "Violation = " << lastViolation << endl;
        if (model.optimiseMode != OptimiseMode::NONE) {
            cout << "objective = "
                 << transposeObjective(optimiseMode, lastObjective) << endl;
        }
        // for experiements
        if (lastViolation == 0) {
            cout << "Best solution: "
                 << transposeObjective(optimiseMode, bestObjective) << " "
                 << getCpuTime() << endl;
        }
    }

    if (vioImproved || (lastViolation == 0 && objImproved)) {
        printCurrentState(model);
    }
}
void StatsContainer::printCurrentState(Model& model) {
    cout << (*this) << "\nTrigger event count " << triggerEventCount << "\n\n";
    if (lastViolation == 0) {
        cout << "solution start\n";
        model.printVariables();
        cout << "solution end\n";
    }
    debug_code(if (debugLogAllowed) {
        debug_log("CSP state:");
        model.csp->dumpState(cout) << endl;
        if (model.optimiseMode != OptimiseMode::NONE) {
            debug_log("Objective state:");
            model.objective->dumpState(cout) << endl;
        }
    });
}

ostream& operator<<(ostream& os, StatsContainer& stats) {
    stats.endTimer();

    os << "Stats:\n";
    os << "Best violation: " << stats.bestViolation << endl;
    os << "Best objective: "
       << transposeObjective(stats.optimiseMode, stats.bestObjective) << endl;

    os << "Number iterations: " << stats.numberIterations << endl;
    os << "Minor node count: " << stats.minorNodeCount << endl;
    chrono::duration<double> timeTaken = stats.endTime - stats.startTime;
    os << "Wall time: " << timeTaken.count() << endl;
    auto cpuTime =
        (double)(stats.endCpuTime - stats.startCpuTime) / CLOCKS_PER_SEC;
    os << "CPU time: " << cpuTime;
    return os;
}
