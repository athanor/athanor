
#ifndef SRC_SEARCH_SOLVER_H_
#define SRC_SEARCH_SOLVER_H_
#include <algorithm>
#include <cassert>
#include <iterator>
#include "search/endOfSearchException.h"
#include "search/model.h"
#include "search/searchStrategies.h"
#include "search/statsContainer.h"
#include "triggers/allTriggers.h"
void signalEndOfSearch();

extern volatile bool sigIntActivated;
extern volatile bool sigAlarmActivated;
extern bool hasIterationLimit;
extern UInt64 iterationLimit;
extern bool hasSolutionLimit;
extern UInt64 solutionLimit;
extern bool runSanityChecks;
extern UInt64 sanityCheckInterval;

inline bool alwaysTrue(const AnyValVec&) { return true; }

inline AnyValVec makeVecFrom(AnyValRef& val) {
    return lib::visit(
        [](auto& val) -> AnyValVec {
            ValRefVec<valType(val)> vec;
            vec.emplace_back(val);
            return vec;
        },
        val);
}

class State {
   public:
    bool disableVarViolations = false;
    Model model;
    ViolationContainer vioContainer;
    StatsContainer stats;
    double totalTimeInNeighbourhoods = 0;
    State(Model model) : model(std::move(model)), stats(this->model) {}

    template <typename ParentStrategy>
    void runNeighbourhood(size_t nhIndex, ParentStrategy&& strategy) {
        testForTermination();
        Neighbourhood& neighbourhood = model.neighbourhoods[nhIndex];
        debug_code(if (debugLogAllowed) {
            debug_log("Iteration count: "
                      << stats.numberIterations
                      << "\nBefore neighbourhood application, state is:");
            stats.printCurrentState(model);
        });
        debug_neighbourhood_action(
            "Applying neighbourhood: " << neighbourhood.name << ":");
        auto& var = model.variables[model.neighbourhoodVarMapping[nhIndex]];
        auto statsMarkPoint = stats.getMarkPoint();
        bool solutionAccepted = false, changeMade = false;
        AcceptanceCallBack callback = [&]() {
            if (runSanityChecks &&
                stats.numberIterations % sanityCheckInterval == 0) {
                model.debugSanityCheck();
            }
            changeMade = true;
            solutionAccepted = strategy(
                NeighbourhoodResult(model, nhIndex, true, statsMarkPoint));
            return solutionAccepted;
        };
        ParentCheckCallBack alwaysTrueFunc(alwaysTrue);
        auto changingVariables = makeVecFrom(var.second);
        NeighbourhoodParams params(callback, alwaysTrueFunc, 1,
                                   changingVariables, *this, vioContainer);
        neighbourhood.apply(params);
        if (runSanityChecks && !solutionAccepted &&
            stats.numberIterations % sanityCheckInterval == 0) {
            model.debugSanityCheck();
        }
        NeighbourhoodResult nhResult(model, nhIndex, changeMade,
                                     statsMarkPoint);
        if (changeMade) {
            updateVarViolations();
        } else {
            // tell strategy that no new assignment found
            strategy(nhResult);
        }
        stats.reportResult(solutionAccepted, nhResult);
        totalTimeInNeighbourhoods +=
            (stats.getRealTime() - nhResult.statsMarkPoint.realTime);
    }
    void testForTermination();
    void updateVarViolations();
};

void dumpVarViolations(const ViolationContainer& vioContainer);
void assignRandomValueToVariables(State& state);
void search(std::shared_ptr<SearchStrategy>& searchStrategy, State& state);
#endif /* SRC_SEARCH_SOLVER_H_ */
