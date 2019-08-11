
#ifndef SRC_SEARCH_TABUSEARCHSTRATEGY_H
#define SRC_SEARCH_TABUSEARCHSTRATEGY_H

template <typename T>
class TabuList {
    std::deque<T> history;
};
template <typename Strategy>
class TabuSearch {
    std::shared_ptr<Strategy> strategy;
    const UInt64 allowedIterationsAtPeak = DEFAULT_PEAK_ITERATIONS;

   public:
    HillClimbing(std::shared_ptr<Strategy> strategy)
        : strategy(std::move(strategy)) {}

    void reset() {}
    void increaseTerminationLimit() {}

    void resetTerminationLimit() {}

    void run(State& state, bool isOuterMostStrategy) {
        UInt64 iterationsAtPeak = 0;
        while (true) {
            bool allowed = false, strictImprovement = false;
            strategy->run(state, [&](const auto& result) {
                if (result.foundAssignment) {
                    if (result.statsMarkPoint.lastViolation != 0) {
                        allowed = result.getDeltaViolation() <= 0;
                        strictImprovement = result.getDeltaViolation() < 0;
                    } else {
                        allowed = result.model.getViolation() == 0 &&
                                  result.getDeltaObjective() <= 0;
                        strictImprovement =
                            allowed && result.getDeltaObjective() < 0;
                    }
                }
                return allowed;
            });
            if (isOuterMostStrategy) {
                continue;
            }
            if (strictImprovement) {
                iterationsAtPeak = 0;
            } else {
                ++iterationsAtPeak;
                if (iterationsAtPeak > allowedIterationsAtPeak) {
                    break;
                }
            }
        }
    }
};

#endif /* SRC_SEARCH_TABUSEARCHSTRATEGY_H */
