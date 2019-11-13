#include "search/statsContainer.h"
#include <iostream>
#include "search/model.h"
#ifdef WASM_TARGET
#include <emscripten/bind.h>
#endif

using namespace std;
extern bool noPrintSolutions;
extern bool quietMode;
Int NeighbourhoodResult::getDeltaViolation() const {
    return model.getViolation() - statsMarkPoint.lastViolation;
}

bool NeighbourhoodResult::objectiveStrictlyBetter() const {
    return model.getObjective() < *statsMarkPoint.lastObjective;
}
bool NeighbourhoodResult::objectiveBetterOrEqual() const {
    return model.getObjective() <= *statsMarkPoint.lastObjective;
}

Int NeighbourhoodResult::getDeltaDefinedness() const {
    bool currentDefined = model.objectiveDefined();
    bool wasDefined = statsMarkPoint.lastObjective.has_value();
    return currentDefined - wasDefined;
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
    : optimiseMode(model.optimiseMode), bestObjective(), lastObjective() {
    for (auto& neighbourhood : model.neighbourhoods) {
        neighbourhoodStats.emplace_back(neighbourhood.name);
    }
}

void StatsContainer::initialSolution(Model& model) {
    lastViolation = model.csp->view()->violation;
    if (model.objectiveDefined()) {
        lastObjective = model.getObjective();
    }
    bestViolation = lastViolation;
    bestObjective = lastObjective;
    checkForBestSolution(true, true, model);
}

#ifdef WASM_TARGET
static const size_t WEB_STATS_REPORT_INTERVAL = 2000;
#endif

// struct to hold information if printing to the athanor web app
class WebAppStats {
    string solutionNumber;
    string wallTime;
    string numberIterations;
    string violation;
    bool _hasObjective;
    string objective;
    // objective is string as this might be tuple of int instead of int
   public:
    WebAppStats(UInt64 solutionNumber, double wallTime, UInt64 numberIterations,
                Int violation, bool hasObjective, string objective)
        : solutionNumber(toString(solutionNumber)),
          wallTime(toString(wallTime)),
          numberIterations(toString(numberIterations)),
          violation(toString(violation)),
          _hasObjective(hasObjective),
          objective(move(objective)) {}

    string getSolutionNumber() const { return solutionNumber; }
    void setSolutionNumber(string solutionNumber) {
        this->solutionNumber = solutionNumber;
    }
    string getWallTime() const { return wallTime; }
    void setWallTime(string wallTime) { this->wallTime = wallTime; }
    string getNumberIterations() const { return numberIterations; }
    void setNumberIterations(string numberIterations) {
        this->numberIterations = numberIterations;
    }
    string getViolation() const { return violation; }
    void setViolation(string violation) { this->violation = violation; }
    bool hasObjective() const { return _hasObjective; }
    void setHasObjective(bool o) { _hasObjective = o; }
    string getObjective() const { return objective; }
    void setObjective(string objective) { this->objective = objective; }
};
#ifdef WASM_TARGET
using namespace emscripten;
EMSCRIPTEN_BINDINGS(WebAppStats) {
    class_<WebAppStats>("WebAppStats")
        .constructor<UInt64, double, UInt64, Int, bool, string>()
        .property("solutionNumber", &WebAppStats::getSolutionNumber,
                  &WebAppStats::setSolutionNumber)
        .property("wallTime", &WebAppStats::getWallTime,
                  &WebAppStats::setWallTime)
        .property("numberIterations", &WebAppStats::getNumberIterations,
                  &WebAppStats::setNumberIterations)
        .property("violation", &WebAppStats::getViolation,
                  &WebAppStats::setViolation)
        .property("hasObjective", &WebAppStats::hasObjective,
                  &WebAppStats::setHasObjective)
        .property("objective", &WebAppStats::getObjective,
                  &WebAppStats::setObjective);
}
#endif

void printStatsToWebApp(const StatsContainer& stats) {
    static_cast<void>(stats);
#ifdef WASM_TARGET
    WebAppStats solutionStats(
        stats.numberBetterFeasibleSolutionsFound, stats.getRealTime(),
        stats.numberIterations, stats.bestViolation,
        stats.optimiseMode != OptimiseMode::NONE &&
            stats.bestObjective.has_value(),
        ((stats.bestObjective) ? toString(*stats.bestObjective) : ""));
    val::global().call<void>("printStats", solutionStats);

#endif
}

void StatsContainer::reportResult(bool solutionAccepted,
                                  const NeighbourhoodResult& result) {
    UInt64 minorNodeCountDiff =
        minorNodeCount - result.statsMarkPoint.minorNodeCount;
    UInt64 triggerEventCountDiff =
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
#ifdef WASM_TARGET
    if (numberIterations % WEB_STATS_REPORT_INTERVAL == 0) {
        printStatsToWebApp(*this);
    }
#endif
    if (!solutionAccepted) {
        return;
    }
    lastViolation = result.model.csp->view()->violation;
    if (result.model.objectiveDefined()) {
        lastObjective = result.model.getObjective();
    } else {
        lastObjective = lib::nullopt;
    }
    bool vioImproved = lastViolation < bestViolation;
    bool objImproved =
        lastObjective && (!bestObjective || *lastObjective < *bestObjective);
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
        printStatsToWebApp(*this);
        cout << "solution end\n";
    }
    debug_code(if (debugLogAllowed) {
        debug_log("CSP state:");
        model.csp->dumpState(cout) << endl;
        if (model.optimiseMode != OptimiseMode::NONE) {
            debug_log("Objective state:");
            lib::visit([&](auto& o) { o->dumpState(cout) << endl; },
                       model.objective);
        }
    });
}

ostream& operator<<(ostream& os, const StatsContainer& stats) {
    os << "Stats:\n";
    os << "Best violation: " << stats.bestViolation << endl;
    os << "Best objective: "
       << ((stats.bestObjective) ? toString(*stats.bestObjective) : "undefined")
       << endl;
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
