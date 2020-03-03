
#ifndef SRC_SEARCH_SOLVER_H_
#define SRC_SEARCH_SOLVER_H_
#include <algorithm>
#include <cassert>
#include <iterator>
#include "search/endOfSearchException.h"
#include "search/model.h"
#include "search/statsContainer.h"
#include "triggers/allTriggers.h"
void signalEndOfSearch();
void dumpVarViolations(const ViolationContainer& vioContainer);
extern volatile bool sigIntActivated;
extern volatile bool sigAlarmActivated;
extern bool hasIterationLimit;
extern UInt64 iterationLimit;
extern bool hasSolutionLimit;
extern UInt64 solutionLimit;
extern bool runSanityChecks;
extern UInt64 sanityCheckInterval;

inline bool alwaysTrue(const AnyValVec&) { return true; }

class State {
   public:
    bool disableVarViolations = false;
    Model model;
    ViolationContainer vioContainer;
    StatsContainer stats;
    double totalTimeInNeighbourhoods = 0;
    State(Model model) : model(std::move(model)), stats(this->model) {}

    auto makeVecFrom(AnyValRef& val) {
        return lib::visit(
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
        static const int attemptsPerNeighbourhood = 30;
        size_t numberAttempts = 0;
        auto statsMarkPoint = stats.getMarkPoint();
        bool solutionAccepted = false, changeMade = false;
        AcceptanceCallBack callback = [&]() {
            if (runSanityChecks &&
                stats.numberIterations % sanityCheckInterval == 0) {
                model.debugSanityCheck();
            }
            changeMade = true;
            NeighbourhoodResult result(model, nhIndex, true, statsMarkPoint);
            bool solutionBetterThanActive =
                (result.statsMarkPoint.lastViolation != 0)
                    ? result.getDeltaViolation() < 0
                    : result.objectiveStrictlyBetter();
            solutionAccepted = false;
            if (solutionBetterThanActive ||
                numberAttempts == attemptsPerNeighbourhood) {
                solutionAccepted = strategy(result);
            }
            return solutionAccepted;
        };
        ParentCheckCallBack alwaysTrueFunc(alwaysTrue);
        auto changingVariables = makeVecFrom(var.second);
        NeighbourhoodParams params(callback, alwaysTrueFunc, 1,
                                   changingVariables, stats, vioContainer);
        do {
            ++numberAttempts;
            neighbourhood.apply(params);
        } while (!solutionAccepted &&
                 numberAttempts < attemptsPerNeighbourhood);
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
        stats.reportResult(solutionAccepted, nhResult, numberAttempts);
        totalTimeInNeighbourhoods +=
            (stats.getRealTime() - nhResult.statsMarkPoint.realTime);
    }

    inline void testForTermination() {
        if (sigIntActivated) {
            std::cout << "control-c pressed\n";
            signalEndOfSearch();
        }
        if (sigAlarmActivated) {
            std::cout << "timeout\n";
            signalEndOfSearch();
        }
        if (hasIterationLimit && stats.numberIterations >= iterationLimit) {
            std::cout << "iteration limit reached\n";
            signalEndOfSearch();
        }

        if (hasSolutionLimit &&
            stats.numberBetterFeasibleSolutionsFound >= solutionLimit) {
            std::cout << "solution limit reached\n";
            signalEndOfSearch();
        }

        if (model.optimiseMode == OptimiseMode::NONE &&
            stats.bestViolation == 0) {
            signalEndOfSearch();
        }
    }

    void updateVarViolations() {
        if (disableVarViolations) {
            return;
        }
        vioContainer.reset();
        if (model.csp->view()->violation == 0) {
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
        if (valBase(var.second).container == &inlinedPool) {
            continue;
        }
        bool success = false;
        auto allocator = mpark::visit(
            [&](auto& d) { return NeighbourhoodResourceAllocator(*d); },
            var.first);
        while (!success) {
            auto resource = allocator.requestLargerResource();
            success =
                assignRandomValueInDomain(var.first, var.second, resource);
            state.stats.minorNodeCount += resource.getResourceConsumed();
        }
    }
}

inline void evaluateAndStartTriggeringDefinedExpressions(State& state) {
    for (auto& nameExprPair : state.model.definingExpressions) {
        lib::visit(
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
    {
        TriggerDepthTracker d;
        evaluateAndStartTriggeringDefinedExpressions(state);
        state.model.csp->evaluate();
        state.model.csp->startTriggering();
        lib::visit(
            [&](auto& objective) {
                objective->evaluate();
                objective->startTriggering();
            },
            state.model.objective);
        handleDefinedVarTriggers();
    }

    if (runSanityChecks) {
        state.model.csp->debugSanityCheck();
        lib::visit([&](auto& objective) { objective->debugSanityCheck(); },
                   state.model.objective);
    }
    state.stats.initialSolution(state.model);
    state.updateVarViolations();
    try {
        if (state.model.neighbourhoods.empty()) {
            signalEndOfSearch();
        }
        searchStrategy->run(state, true);
    } catch (EndOfSearchException&) {
    }
}

#endif /* SRC_SEARCH_SOLVER_H_ */
