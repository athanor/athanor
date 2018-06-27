#ifndef SRC_SEARCH_STATSCONTAINER_H_
#define SRC_SEARCH_STATSCONTAINER_H_
#include <chrono>
#include <iostream>
#include "search/model.h"

struct StatsMarkPoint {
    u_int64_t numberIterations;
    u_int64_t minorNodeCount;
    u_int64_t triggerEventCount;
    double cpuTime;
    UInt bestViolation;
    UInt lastViolation;
    Int bestObjective;
    Int lastObjective;

    StatsMarkPoint(u_int64_t numberIterations, u_int64_t minorNodeCount,
                   u_int64_t triggerEventCount, double cpuTime,
                   UInt bestViolation, UInt lastViolation, Int bestObjective,
                   Int lastObjective)
        : numberIterations(numberIterations),
          minorNodeCount(minorNodeCount),
          triggerEventCount(triggerEventCount),
          cpuTime(cpuTime),
          bestViolation(bestViolation),
          lastViolation(lastViolation),
          bestObjective(bestObjective),
          lastObjective(lastObjective) {}
};

inline Int calcDeltaObjective(OptimiseMode mode, Int newObj, Int oldObj) {
    switch (mode) {
        case OptimiseMode::MAXIMISE:
            return oldObj - newObj;
        case OptimiseMode::MINIMISE:
            return newObj - oldObj;
        default:
            return 0;
    }
}

struct NeighbourhoodResult {
    Model& model;
    size_t neighbourhoodIndex;
    StatsMarkPoint statsMarkPoint;

    NeighbourhoodResult(Model& model, size_t neighbourhoodIndex,
                        const StatsMarkPoint& statsMarkPoint)
        : model(model),
          neighbourhoodIndex(neighbourhoodIndex),
          statsMarkPoint(statsMarkPoint) {}

    inline UInt getViolation() const { return model.csp->view().violation; }
    inline Int getObjective() const { return model.objective->view().value; }
    inline Int getDeltaViolation() const {
        return getViolation() - statsMarkPoint.lastViolation;
    }
    inline Int getDeltaObjective() const {
        return calcDeltaObjective(model.optimiseMode, getObjective(),
                                  statsMarkPoint.lastObjective);
    }
};

struct StatsContainer {
    u_int64_t numberIterations = 0;
    u_int64_t minorNodeCount = 0;
    std::chrono::high_resolution_clock::time_point startTime =
        std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point endTime;
    std::clock_t startCpuTime = std::clock();
    std::clock_t endCpuTime;
    std::vector<u_int64_t> nhActivationCounts;
    std::vector<u_int64_t> nhMinorNodeCounts;
    std::vector<u_int64_t> nhTriggerEventCounts;
    std::vector<double> nhTotalCpuTimes;
    UInt bestViolation;
    UInt lastViolation;
    Int bestObjective;
    Int lastObjective;
    StatsContainer(Model& model)
        : nhActivationCounts(model.neighbourhoods.size(), 0),
          nhMinorNodeCounts(model.neighbourhoods.size(), 0),
          nhTriggerEventCounts(model.neighbourhoods.size(), 0),
          nhTotalCpuTimes(model.neighbourhoods.size(), 0) {}

    void initialSolution(Model& model) {
        lastViolation = model.csp->view().violation;
        lastObjective = model.objective->view().value;
        bestViolation = lastViolation;
        bestObjective = lastObjective;
        checkForBestSolution(true, true, model);
    }

    inline void reportResult(const NeighbourhoodResult& result) {
        ++numberIterations;
        ++nhActivationCounts[result.neighbourhoodIndex];
        nhMinorNodeCounts[result.neighbourhoodIndex] +=
            minorNodeCount - result.statsMarkPoint.minorNodeCount;
        nhTriggerEventCounts[result.neighbourhoodIndex] +=
            triggerEventCount - result.statsMarkPoint.triggerEventCount;
        nhTotalCpuTimes[result.neighbourhoodIndex] +=
            getCpuTime() - result.statsMarkPoint.cpuTime;
        lastViolation = result.model.csp->view().violation;
        lastObjective = result.model.objective->view().value;
        bool vioImproved = lastViolation < bestViolation,
             objImproved = calcDeltaObjective(result.model.optimiseMode,
                                              lastObjective, bestObjective) < 0;
        checkForBestSolution(vioImproved, objImproved, result.model);
    }

    void checkForBestSolution(bool vioImproved, bool objImproved,
                              Model& model) {
        // following is a bit messy for printing purposes
        if (vioImproved) {
            bestViolation = lastViolation;
        }
        if ((bestViolation == 0 && objImproved) ||
            (bestViolation != 0 && vioImproved)) {
            bestObjective = lastObjective;
        }
        if (vioImproved || objImproved) {
            std::cout << "\nNew solution:\n";
            std::cout << "Violation = " << lastViolation << std::endl;
            if (model.optimiseMode != OptimiseMode::NONE) {
                std::cout << "objective = " << lastObjective << std::endl;
            }
            // for experiements
            if (bestViolation == 0) {
                std::cout << "Best solution: " << bestObjective << " "
                          << getCpuTime() << std::endl;
            }
        }

        if (vioImproved || objImproved) {
            printCurrentState(model);
        }
    }
    inline void printCurrentState(Model& model) {
        std::cout << (*this) << "\nTrigger event count " << triggerEventCount
                  << "\n\n";

        if (lastViolation == 0) {
            std::cout << "solution start\n";
        }
        model.printVariables();
        if (lastViolation == 0) {
            std::cout << "solution end\n";
        }
        debug_code(debug_log("CSP state:");
                   model.csp->dumpState(std::cout) << std::endl;
                   if (model.optimiseMode != OptimiseMode::NONE) {
                       debug_log("Objective state:");
                       model.objective->dumpState(std::cout) << std::endl;
                   });
    }

    inline StatsMarkPoint getMarkPoint() {
        return StatsMarkPoint(numberIterations, minorNodeCount,
                              triggerEventCount, getCpuTime(), bestViolation,
                              lastViolation, bestObjective, lastObjective);
    }
    inline void startTimer() {
        startTime = std::chrono::high_resolution_clock::now();
        startCpuTime = std::clock();
    }
    inline void endTimer() {
        endTime = std::chrono::high_resolution_clock::now();
        endCpuTime = std::clock();
    }

    friend inline std::ostream& operator<<(std::ostream& os,
                                           StatsContainer& stats) {
        stats.endTimer();

        os << "Stats:\n";
        os << "Best violation: " << stats.bestViolation << std::endl;
        os << "Best objective: " << stats.bestObjective << std::endl;

        os << "Number iterations: " << stats.numberIterations << std::endl;
        os << "Minor node count: " << stats.minorNodeCount << std::endl;
        std::chrono::duration<double> timeTaken =
            stats.endTime - stats.startTime;
        os << "Wall time: " << timeTaken.count() << std::endl;
        auto cpuTime =
            (double)(stats.endCpuTime - stats.startCpuTime) / CLOCKS_PER_SEC;
        os << "CPU time: " << cpuTime;
        return os;
    }
    inline double getAverage(double value, size_t index) {
        return value / nhActivationCounts[index];
    }
    double getCpuTime() {
        endTimer();
        return (double)(endCpuTime - startCpuTime) / CLOCKS_PER_SEC;
    }
};

#endif /* SRC_SEARCH_STATSCONTAINER_H_ */
