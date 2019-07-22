#include "search/statsContainer.h"
#include <iostream>
#include "search/model.h"

using namespace std;
extern bool noPrintSolutions;
extern bool quietMode;
Int NeighbourhoodResult::getDeltaViolation() const {
    return model.getViolation() - statsMarkPoint.lastViolation;
}

bool NeighbourhoodResult::objectiveStrictlyBetter() const {
    return model.getObjective() < statsMarkPoint.lastObjective;
}
bool NeighbourhoodResult::objectiveBetterOrEqual() const {
    return model.getObjective() <= statsMarkPoint.lastObjective;
}

Int NeighbourhoodResult::getDeltaDefinedness() const {
    bool currentDefined = model.objectiveDefined();
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
       << stats.numberVioImprovements << ","
       << stats.getAverage(stats.numberVioImprovements) << endl;
    os << indent << "Number minor nodes," << stats.minorNodeCount << ","
       << stats.getAverage(stats.minorNodeCount) << endl;
    os << indent << "Number trigger events," << stats.triggerEventCount << ","
       << stats.getAverage(stats.triggerEventCount) << endl;
    os << indent << "Real time," << stats.totalRealTime << ","
       << stats.getAverage(stats.totalRealTime);
    return os;
}

StatsContainer::StatsContainer(Model& model)
    : optimiseMode(model.optimiseMode),
      bestObjective(optimiseMode, make<IntValue>().asExpr()),
      lastObjective(bestObjective) {
    for (auto& neighbourhood : model.neighbourhoods) {
        neighbourhoodStats.emplace_back(neighbourhood.name);
    }
}

void StatsContainer::initialSolution(Model& model) {
    lastViolation = model.csp->view()->violation;
    lastObjectiveDefined = model.objectiveDefined();
    if (lastObjectiveDefined) {
        lastObjective = model.getObjective();
    }
    bestViolation = lastViolation;
    bestObjective = lastObjective;
    checkForBestSolution(true, true, model);
}

void StatsContainer::reportResult(bool solutionAccepted,
                                  const NeighbourhoodResult& result) {
    u_int64_t minorNodeCountDiff =
        minorNodeCount - result.statsMarkPoint.minorNodeCount;
    u_int64_t triggerEventCountDiff =
        triggerEventCount - result.statsMarkPoint.triggerEventCount;
    ++numberIterations;
    ++neighbourhoodStats[result.neighbourhoodIndex].numberActivations;
    neighbourhoodStats[result.neighbourhoodIndex].minorNodeCount +=
        minorNodeCountDiff;
    neighbourhoodStats[result.neighbourhoodIndex].triggerEventCount +=
        triggerEventCountDiff;
    double timeDiff = getRealTime() - result.statsMarkPoint.realTime;
    neighbourhoodStats[result.neighbourhoodIndex].totalRealTime += timeDiff;
    neighbourhoodStats[result.neighbourhoodIndex].numberObjImprovements +=
        (result.statsMarkPoint.lastViolation == 0 &&
         result.model.getViolation() == 0 && result.objectiveStrictlyBetter());
    if (result.statsMarkPoint.lastViolation > 0) {
        ++numberVioIterations;
        vioMinorNodeCount += minorNodeCountDiff;
        vioTriggerEventCount += triggerEventCountDiff;
        ++neighbourhoodStats[result.neighbourhoodIndex].numberVioActivations;
        neighbourhoodStats[result.neighbourhoodIndex].vioMinorNodeCount +=
            minorNodeCountDiff;
        neighbourhoodStats[result.neighbourhoodIndex].vioTriggerEventCount +=
            triggerEventCountDiff;
        neighbourhoodStats[result.neighbourhoodIndex].vioTotalRealTime +=
            timeDiff;
        vioTotalTime += timeDiff;
    }
    neighbourhoodStats[result.neighbourhoodIndex].numberVioImprovements +=
        (result.getDeltaViolation() < 0);
    if (!solutionAccepted) {
        return;
    }
    lastViolation = result.model.csp->view()->violation;
    lastObjectiveDefined = result.model.objectiveDefined();
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

    // track number of better feasible solutions.
    // check for the most recent violation being 0 and that either this is the
    // first feasible solution, or that this feasible solution has a better
    // objective.

    if ((bestViolation != 0 &&
         lastViolation == 0) ||  // first feasible solution
        (bestViolation == 0 && lastViolation == 0 &&
         objImproved)  // better feasible solution
    ) {
        ++numberBetterFeasibleSolutionsFound;
    }

    // If there has been any improvement, store the time.
    if ((bestViolation != 0 && vioImproved) ||
        (bestViolation == 0 && lastViolation == 0 && objImproved)) {
        tie(cpuTimeTillBestSolution, realTimeTillBestSolution) = getTime();
    }

    // track best violation
    if (vioImproved) {
        bestViolation = lastViolation;
    }

    // track best objective
    // either the best violation is at 0, in which case the most recent
    // violation must be 0 and the objective has improved, or the best violation
    // is still not 0, in which case, an improvement to the violation is an
    // improvement to the objective.
    if ((bestViolation == 0 && lastViolation == 0 && objImproved) ||
        (bestViolation != 0 && vioImproved)) {
        bestObjective = lastObjective;
    }

    if (vioImproved || (lastViolation == 0 && objImproved)) {
        printCurrentState(model);
    }
}

void StatsContainer::printCurrentState(Model& model) {
    if (!quietMode) {
        cout << (*this) << "\nTrigger event count " << triggerEventCount
             << "\n\n";
    }
    if (!noPrintSolutions && lastViolation == 0) {
        cout << "solution start\n";
        model.printVariables();
        cout << "solution end\n";
    }
    debug_code(if (debugLogAllowed) {
        debug_log("CSP state:");
        model.csp->dumpState(cout) << endl;
        if (model.optimiseMode != OptimiseMode::NONE) {
            debug_log("Objective state:");
            mpark::visit([&](auto& o) { o->dumpState(cout) << endl; },
                         model.objective);
        }
    });
}

ostream& operator<<(ostream& os, const StatsContainer& stats) {
    os << "Stats:\n";
    os << "Best violation: " << stats.bestViolation << endl;
    os << "Best objective: " << stats.bestObjective << endl;
    os << "CPU time till best solution: " << stats.cpuTimeTillBestSolution
       << endl;
    os << "Real time till best solution: " << stats.realTimeTillBestSolution
       << endl;
    os << "Number solutions: " << stats.numberBetterFeasibleSolutionsFound
       << endl;
    os << "Number iterations: " << stats.numberIterations << endl;
    os << "Minor node count: " << stats.minorNodeCount;
    return os;
}

void StatsContainer::printNeighbourhoodStats(std::ostream& os) const {
    csvRow(os, "name", "activations", "objImprovements",
           "objImprovementsAverage", "vioImprovements",
           "vioImprovementsAverage", "minorNodes", "minorNodesAverage",
           "triggerEvents", "triggerEventsAverage", "realTime",
           "realTimeAverage");
    for (auto& s : neighbourhoodStats) {
        double averageObjImprovements =
            (s.numberObjImprovements == 0)
                ? 0
                : ((double)s.numberObjImprovements) /
                      (s.numberActivations - s.numberVioActivations);
        double averageVioImprovements =
            (s.numberVioImprovements == 0)
                ? 0
                : ((double)s.numberVioImprovements) / s.numberVioActivations;
        csvRow(os, s.name, s.numberActivations, s.numberObjImprovements,
               averageObjImprovements, s.numberVioImprovements,
               averageVioImprovements, s.minorNodeCount,
               s.getAverage(s.minorNodeCount), s.triggerEventCount,
               s.getAverage(s.triggerEventCount), s.totalRealTime,
               s.getAverage(s.totalRealTime));
    }
}
