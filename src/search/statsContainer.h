#ifndef SRC_SEARCH_STATSCONTAINER_H_
#define SRC_SEARCH_STATSCONTAINER_H_
#include <chrono>
#include <iostream>
struct Model;
struct NeighbourhoodResult {
    Model& model;
    UInt lastViolation;
    size_t neighbourhoodIndex;
    u_int64_t numberMinorNodes;
    u_int64_t numberTriggers;
    double cpuTimeTaken;
    NeighbourhoodResult(Model& model, UInt lastViolation,
                        size_t neighbourhoodIndex, u_int64_t numberMinorNodes,
                        u_int64_t numberTriggers, double cpuTimeTaken)
        : model(model),
          lastViolation(lastViolation),
          neighbourhoodIndex(neighbourhoodIndex),
          numberMinorNodes(numberMinorNodes),
          numberTriggers(numberTriggers),
          cpuTimeTaken(cpuTimeTaken) {}
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

    StatsContainer(size_t numberNeighbourhoods)
        : nhActivationCounts(numberNeighbourhoods, 0),
          nhMinorNodeCounts(numberNeighbourhoods, 0),
          nhTriggerEventCounts(numberNeighbourhoods, 0),
          nhTotalCpuTimes(numberNeighbourhoods, 0) {}
    void reportResult(const NeighbourhoodResult& result) {
        ++numberIterations;
        ++nhActivationCounts[result.neighbourhoodIndex];
        nhMinorNodeCounts[result.neighbourhoodIndex] += result.numberMinorNodes;
        nhTriggerEventCounts[result.neighbourhoodIndex] +=
            result.numberTriggers;
        nhTotalCpuTimes[result.neighbourhoodIndex] += result.cpuTimeTaken;
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
