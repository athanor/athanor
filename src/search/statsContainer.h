#ifndef SRC_SEARCH_STATSCONTAINER_H_
#define SRC_SEARCH_STATSCONTAINER_H_
#include <chrono>
#include <iostream>
struct StatsContainer {
    size_t majorNodeCount = 0;
    size_t minorNodeCount = 0;
    std::chrono::high_resolution_clock::time_point startTime =
        std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point endTime;
    std::clock_t startCpuTime = std::clock();
    std::clock_t endCpuTime;

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
        os << "Major node count: " << stats.majorNodeCount << std::endl;
        os << "Minor node count: " << stats.minorNodeCount << std::endl;
        std::chrono::duration<double> timeTaken =
            stats.endTime - stats.startTime;
        os << "Wall time: " << timeTaken.count() << std::endl;
        auto cpuTime =
            (double)(stats.endCpuTime - stats.startCpuTime) / CLOCKS_PER_SEC;
        os << "CPU time: " << cpuTime;
        return os;
    }

    double getCpuTime() {
        endTimer();
        return (double)(endCpuTime - startCpuTime) / CLOCKS_PER_SEC;
    }
};

#endif /* SRC_SEARCH_STATSCONTAINER_H_ */
