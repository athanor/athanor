#include "search/solver.h"

void assignRandomValueToVariables(State& state) {
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

void State::testForTermination() {
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

    if (model.optimiseMode == OptimiseMode::NONE && stats.bestViolation == 0) {
        signalEndOfSearch();
    }
}

void State::updateVarViolations() {
    if (disableVarViolations) {
        return;
    }
    vioContainer.reset();
    if (model.csp->view()->violation == 0) {
        return;
    }
    model.csp->updateVarViolations(0, vioContainer);
}

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
