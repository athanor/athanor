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
    double realTime;
    UInt bestViolation;
    UInt lastViolation;
    Int bestObjective;
    Int lastObjective;
    bool lastObjectiveDefined;

    StatsMarkPoint(u_int64_t numberIterations, u_int64_t minorNodeCount,
                   u_int64_t triggerEventCount, double realTime,
                   UInt bestViolation, UInt lastViolation, Int bestObjective,
                   Int lastObjective, bool lastObjectiveDefined)
        : numberIterations(numberIterations),
          minorNodeCount(minorNodeCount),
          triggerEventCount(triggerEventCount),
          realTime(realTime),
          bestViolation(bestViolation),
          lastViolation(lastViolation),
          bestObjective(bestObjective),
          lastObjective(lastObjective),
          lastObjectiveDefined(lastObjectiveDefined) {}
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
    Int getDeltaDefinedness() const;
};

#ifndef CLASS_OPTIMISEMODE
#define CLASS_OPTIMISEMODE
enum class OptimiseMode { NONE, MAXIMISE, MINIMISE };
#endif

struct NeighbourhoodStats {
    std::string name;
    u_int64_t numberActivations = 0;
    u_int64_t minorNodeCount = 0;
    u_int64_t triggerEventCount = 0;
    double totalRealTime = 0;
    double vioTotalRealTime = 0;
    u_int64_t numberVioActivations = 0;
    u_int64_t vioMinorNodeCount = 0;
    u_int64_t vioTriggerEventCount = 0;
    u_int64_t numberObjImprovements = 0;
    u_int64_t numberVioImprovmenents = 0;

    NeighbourhoodStats(const std::string& name) : name(name) {}
    inline double getAverage(double value) const {
        return value / numberActivations;
    }
    friend std::ostream& operator<<(std::ostream& os,
                                    const NeighbourhoodStats& stats);
};

struct StatsContainer {
    OptimiseMode optimiseMode;
    u_int64_t numberIterations = 0;
    u_int64_t numberVioIterations = 0;
    u_int64_t minorNodeCount = 0;
    u_int64_t vioMinorNodeCount = 0;
    std::chrono::high_resolution_clock::time_point startTime =
        std::chrono::high_resolution_clock::now();
    std::clock_t startCpuTime = std::clock();
    double cpuTimeTillBestSolution;
    double realTimeTillBestSolution;
    UInt bestViolation;
    UInt lastViolation;
    Int bestObjective;
    Int lastObjective;
    bool lastObjectiveDefined;
    std::vector<NeighbourhoodStats> neighbourhoodStats;

    StatsContainer(Model& model);

    inline StatsMarkPoint getMarkPoint() {
        return StatsMarkPoint(numberIterations, minorNodeCount,
                              triggerEventCount, getRealTime(), bestViolation,
                              lastViolation, bestObjective, lastObjective,
                              lastObjectiveDefined);
    }
    inline void startTimer() {
        startTime = std::chrono::high_resolution_clock::now();
        startCpuTime = std::clock();
    }
    inline auto getOsTime() {
        return std::chrono::high_resolution_clock::now();
    }

    inline double getRealTime() {
        return std::chrono::duration<double>(getOsTime() - startTime).count();
    }

    void initialSolution(Model& model);
    void checkForBestSolution(bool vioImproved, bool objImproved, Model& model);
    void reportResult(bool solutionAccepted, const NeighbourhoodResult& result);
    void printCurrentState(Model& model);
    friend std::ostream& operator<<(std::ostream& os,
                                    const StatsContainer& stats);

    std::pair<double, double> getTime() {
        double realTime = getRealTime();
        double cpuTime = (double)(std::clock() - startCpuTime) / CLOCKS_PER_SEC;
        return std::make_pair(cpuTime, realTime);
    }
};

#endif /* SRC_SEARCH_STATSCONTAINER_H_ */
