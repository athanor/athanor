#ifndef SRC_SEARCH_STATSCONTAINER_H_
#define SRC_SEARCH_STATSCONTAINER_H_
#include <chrono>
#include <iostream>
#include "base/base.h"
#include "search/objective.h"
struct Model;
struct StatsMarkPoint {
    UInt64 numberIterations;
    UInt64 minorNodeCount;
    UInt64 triggerEventCount;
    double realTime;
    UInt bestViolation;
    UInt lastViolation;
    lib::optional<Objective> bestObjective;
    lib::optional<Objective> lastObjective;

    StatsMarkPoint(UInt64 numberIterations, UInt64 minorNodeCount,
                   UInt64 triggerEventCount, double realTime,
                   UInt bestViolation, UInt lastViolation,
                   lib::optional<Objective> bestObjective,
                   lib::optional<Objective> lastObjective)
        : numberIterations(numberIterations),
          minorNodeCount(minorNodeCount),
          triggerEventCount(triggerEventCount),
          realTime(realTime),
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
    bool objectiveStrictlyBetter() const;
    bool objectiveBetterOrEqual() const;
    Int getDeltaDefinedness() const;
};

struct NeighbourhoodStats {
    std::string name;
    UInt64 numberActivations = 0;
    UInt64 numberAttempts = 0;
    UInt64 minorNodeCount = 0;
    UInt64 triggerEventCount = 0;
    double totalRealTime = 0;
    double vioTotalRealTime = 0;
    UInt64 numberVioActivations = 0;
    UInt64 vioMinorNodeCount = 0;
    UInt64 vioTriggerEventCount = 0;
    UInt64 numberObjImprovements = 0;
    UInt64 numberVioImprovements = 0;

    NeighbourhoodStats(const std::string& name) : name(name) {}
    inline double getAverage(double value) const {
        if (value == 0 && numberActivations == 0) {
            return 0;
            // maybe should be 1, I prefer 0
        }
        return value / numberActivations;
    }

    friend std::ostream& operator<<(std::ostream& os,
                                    const NeighbourhoodStats& stats);
};

struct StatsContainer {
    OptimiseMode optimiseMode;
    UInt64 numberIterations = 0;
    UInt64 numberVioIterations = 0;
    UInt64 minorNodeCount = 0;
    UInt64 vioMinorNodeCount = 0;
    UInt64 vioTriggerEventCount = 0;
    UInt64 numberBetterFeasibleSolutionsFound = 0;
    std::chrono::high_resolution_clock::time_point startTime =
        std::chrono::high_resolution_clock::now();
    std::clock_t startCpuTime = std::clock();
    double cpuTimeTillBestSolution;
    double realTimeTillBestSolution;
    double vioTotalTime = 0;
    UInt bestViolation;
    UInt lastViolation;
    lib::optional<Objective> bestObjective;
    lib::optional<Objective> lastObjective;
    std::vector<NeighbourhoodStats> neighbourhoodStats;

    StatsContainer(Model& model);

    inline StatsMarkPoint getMarkPoint() {
        return StatsMarkPoint(numberIterations, minorNodeCount,
                              triggerEventCount, getRealTime(), bestViolation,
                              lastViolation, bestObjective, lastObjective);
    }
    inline void startTimer() {
        startTime = std::chrono::high_resolution_clock::now();
        startCpuTime = std::clock();
    }
    inline auto getOsTime() const {
        return std::chrono::high_resolution_clock::now();
    }

    inline double getRealTime() const {
        return std::chrono::duration<double>(getOsTime() - startTime).count();
    }

    void initialSolution(Model& model);
    void checkForBestSolution(bool vioImproved, bool objImproved, Model& model);
    void reportResult(bool solutionAccepted, const NeighbourhoodResult& result, int numberAttemptss);
    void printCurrentState(Model& model);
    friend std::ostream& operator<<(std::ostream& os,
                                    const StatsContainer& stats);

    std::pair<double, double> getTime() const {
        double realTime = getRealTime();
        double cpuTime = (double)(std::clock() - startCpuTime) / CLOCKS_PER_SEC;
        return std::make_pair(cpuTime, realTime);
    }

    void printNeighbourhoodStats(std::ostream& os) const;
};

template <typename... Args>
void csvRow(std::ostream& os, Args&&... args) {
    bool first = true;
    auto print = [&](auto&& arg) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        os << arg;
    };
    int unpack[]{0, (print(args), 0)...};
    static_cast<void>(unpack);
    os << "\n";
}

#endif /* SRC_SEARCH_STATSCONTAINER_H_ */
