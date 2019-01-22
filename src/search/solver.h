
#ifndef SRC_SEARCH_SOLVER_H_
#define SRC_SEARCH_SOLVER_H_
#include <algorithm>
#include <cassert>
#include <iterator>
#include "search/model.h"
#include "search/statsContainer.h"
#include "triggers/allTriggers.h"
void dumpVarViolations(const ViolationContainer& vioContainer);
extern volatile bool sigIntActivated;
extern volatile bool sigAlarmActivated;
extern u_int64_t iterationLimit;
extern bool runSanityChecks;
inline bool alwaysTrue(const AnyValVec&) { return true; }

struct EndOfSearchException {};
class State {
   public:
    bool disableVarViolations = false;
    Model model;
    ViolationContainer vioContainer;
    StatsContainer stats;
    double totalTimeInNeighbourhoods = 0;
    State(Model model) : model(std::move(model)), stats(this->model) {}

    auto makeVecFrom(AnyValRef& val) {
        return mpark::visit(
            [](auto& val) -> AnyValVec {
                ValRefVec<valType(val)> vec;
                vec.emplace_back(val);
                return vec;
            },
            val);
    }

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
            if (runSanityChecks) {
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
                                   changingVariables, stats, vioContainer);
        neighbourhood.apply(params);
        if (runSanityChecks && !solutionAccepted) {
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

    inline void testForTermination() {
        if (sigIntActivated) {
            std::cout << "control-c pressed\n";
            throw EndOfSearchException();
        }
        if (sigAlarmActivated) {
            std::cout << "timeout\n";
            throw EndOfSearchException();
        }
        if (iterationLimit != 0 && stats.numberIterations >= iterationLimit) {
            std::cout << "iteration limit reached\n";
            throw EndOfSearchException();
        }
        if (model.optimiseMode == OptimiseMode::NONE &&
            stats.bestViolation == 0) {
            throw EndOfSearchException();
        }
    }

    void printNeighbourhoodStats() {
        std::cout << "---------------------\n";
        for (auto& nhStats : stats.neighbourhoodStats) {
            std::cout << nhStats << std::endl;
        }
    }
    void updateVarViolations() {
        if (disableVarViolations) {
            return;
        }
        vioContainer.reset();
        if (model.csp->violation == 0) {
            return;
        }
        model.csp->updateVarViolations(0, vioContainer);
    }
};

void dumpVarViolations(const ViolationContainer& vioContainer) {
    auto sortedVars = vioContainer.getVarsWithViolation();
    std::sort(sortedVars.begin(), sortedVars.end());
    for (auto& var : sortedVars) {
        std::cout << "var: " << var
                  << ", violation=" << vioContainer.varViolation(var)
                  << std::endl;
    }
    for (auto& var : sortedVars) {
        if (vioContainer.hasChildViolation(var)) {
            std::cout << "Entering var " << var << std::endl;
            dumpVarViolations(vioContainer.childViolations(var));
            std::cout << "exiting var " << var << std::endl;
        }
    }
}
inline void assignRandomValueToVariables(State& state) {
    for (auto& var : state.model.variables) {
        if (valBase(var.second).container == &definedPool) {
            continue;
        }
        assignRandomValueInDomain(var.first, var.second, state.stats);
    }
}

inline void evaluateAndStartTriggeringDefinedExpressions(State& state) {
    for (auto& nameExprPair : state.model.definingExpressions) {
        mpark::visit(
            [&](auto& expr) {
                expr->evaluate();
                expr->startTriggering();
            },
            nameExprPair.second);
    }
}

template <typename SearchStrategy>
void search(std::shared_ptr<SearchStrategy>& searchStrategy, State& state) {
    triggerEventCount = 0;
    std::cout << "Neighbourhoods (" << state.model.neighbourhoods.size()
              << "):\n";
    std::transform(state.model.neighbourhoods.begin(),
                   state.model.neighbourhoods.end(),
                   std::ostream_iterator<std::string>(std::cout, "\n"),
                   [](auto& n) -> std::string& { return n.name; });

    state.stats.startTimer();
    assignRandomValueToVariables(state);
    evaluateAndStartTriggeringDefinedExpressions(state);
    state.model.csp->evaluate();
    state.model.csp->startTriggering();

    state.model.objective->evaluate();
    state.model.objective->startTriggering();
    if (runSanityChecks) {
        state.model.csp->debugSanityCheck();
        state.model.objective->debugSanityCheck();
    }
    state.stats.initialSolution(state.model);
    state.updateVarViolations();
    try {
        searchStrategy->run(state, true);
    } catch (EndOfSearchException&) {
    }
    std::cout << "\n\n";
    state.printNeighbourhoodStats();
    std::cout << "\n\n"
              << state.stats << "\nTrigger event count " << triggerEventCount
              << "\n";
    auto times = state.stats.getTime();
    std::cout << "total real time actually spent in neighbourhoods: "
              << state.totalTimeInNeighbourhoods << std::endl;
    std::cout << "Total CPU time: " << times.first << std::endl;
    std::cout << "Total real time: " << times.second << std::endl;
}

#endif /* SRC_SEARCH_SOLVER_H_ */
