#ifndef SRC_SEARCH_STATSCONTAINER_H_
#define SRC_SEARCH_STATSCONTAINER_H_
#include <chrono>
#include <iostream>
#include "base/base.h"
struct Model;
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

struct NeighbourhoodResult {
    Model& model;
    size_t neighbourhoodIndex;
    bool foundAssignment;
    StatsMarkPoint statsMarkPoint;
    NeighbourhoodResult(Model& model, size_t neighbourhoodIndex,
                        bool foundAssignment,
                        const StatsMarkPoint& statsMarkPoint)
        : model(model),
          neighbourhoodIndex(neighbourhoodIndex),
          foundAssignment(foundAssignment),
          statsMarkPoint(statsMarkPoint) {}

    Int getDeltaViolation() const;
    Int getDeltaObjective() const;
};
#ifndef CLASS_OPTIMISEMODE
#define CLASS_OPTIMISEMODE
enum class OptimiseMode { NONE, MAXIMISE, MINIMISE };
#endif

struct StatsContainer {
    OptimiseMode optimiseMode;
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
    StatsContainer(Model& model);

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
    void initialSolution(Model& model);
    void checkForBestSolution(bool vioImproved, bool objImproved, Model& model);
    void reportResult(bool solutionAccepted, const NeighbourhoodResult& result);
    void printCurrentState(Model& model);
    friend std::ostream& operator<<(std::ostream& os, StatsContainer& stats);
    inline double getAverage(double value, size_t index) {
        return value / nhActivationCounts[index];
    }
    double getCpuTime() {
        endTimer();
        return (double)(endCpuTime - startCpuTime) / CLOCKS_PER_SEC;
    }
};

#endif /* SRC_SEARCH_STATSCONTAINER_H_ */
