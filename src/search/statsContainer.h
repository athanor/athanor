#ifndef SRC_SEARCH_STATSCONTAINER_H_
#define SRC_SEARCH_STATSCONTAINER_H_
#include <chrono>
#include <iostream>
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
    StatsMarkPoint statsMarkPoint;

    NeighbourhoodResult(Model& model, size_t neighbourhoodIndex,
                        const StatsMarkPoint& statsMarkPoint)
        : model(model),
          neighbourhoodIndex(neighbourhoodIndex),
          statsMarkPoint(statsMarkPoint) {}

    Int getDeltaViolation() {
        return model.csp->view().violation - statsMarkPoint.lastViolation;
    }
    Int getDeltaObjective() {
        return (model.optimiseMode != OptimiseMode::NONE)
                   ? model.objective->view()->value -
                         statsMarkPoint.lastObjective
                   : 0;
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

    void reportResult(const NeighbourhoodResult& result) {
        ++numberIterations;
        ++nhActivationCounts[result.neighbourhoodIndex];
        nhMinorNodeCounts[result.neighbourhoodIndex] += minorNodeCount - result.statsMarkPoint.numberMinorNodes;
        nhTriggerEventCounts[result.neighbourhoodIndex] +=
            triggerEventCount - result.statsMarkPoint.numberTriggers;
        nhTotalCpuTimes[result.neighbourhoodIndex] += getCpuTime() - result.statsMarkPoint.cpuTimeTaken;
    }

    StatsMarkPoint getMarkPoint() {
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
        os << "Stats:\n";
        stats.endTimer();
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
