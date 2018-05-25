
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <cassert>
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/statsContainer.h"
#include "types/allTypes.h"
void dumpVarViolations(const ViolationContainer& vioDesc);
extern bool sigIntActivated, sigAlarmActivated;
inline bool alwaysTrue(const AnyValRef&) { return true; }

template <typename NeighbourhoodSelectionStrategy>
class HillClimber {
    Model model;
    NeighbourhoodSelectionStrategy selectionStrategy;
    UInt lastViolation;
    UInt violationBackOff = 1;
    bool otherPhase = false;
    UInt bestViolation;
    Int lastObjValue;
    Int bestObjValue;
    bool newBestValueFound = false;
    StatsContainer stats;
    UInt numberItersAtLastEvent;

    Int getDeltaObj(Int oldObj, Int newObj) {
        switch (model.optimiseMode) {
            case OptimiseMode::MAXIMISE:
                return newObj - oldObj;
            case OptimiseMode::MINIMISE:
                return oldObj - newObj;
            case OptimiseMode::NONE:
                return 0;
            default:
                assert(false);
                abort();
        }
    }
    inline bool acceptValue() {
        UInt newObjValue = model.objective->view().value;
        Int deltaObj = getDeltaObj(lastObjValue, newObjValue);
        Int deltaViolation = lastViolation - model.csp->violation;
        bool solutionAllowed;
        if (!otherPhase) {
            solutionAllowed =
                deltaViolation > 0 || (deltaViolation == 0 && deltaObj >= 0);
        } else {
            solutionAllowed = deltaViolation >= 0 &&
                              getDeltaObj(bestObjValue, newObjValue) > 0;
            if (solutionAllowed && model.csp->violation == 0) {
                otherPhase = false;
                numberItersAtLastEvent = stats.numberIterations;
            }
        }
        if (!solutionAllowed) {
            return false;
        } else {
            if (deltaViolation > 0 || (deltaViolation == 0 && deltaObj > 0)) {
                numberItersAtLastEvent = stats.numberIterations;
            }

            lastViolation = model.csp->violation;
            if (model.optimiseMode != OptimiseMode::NONE) {
                lastObjValue = newObjValue;
            }
            Int deltaBestViolation = bestViolation - lastViolation;
            Int deltaBestObj = getDeltaObj(bestObjValue, lastObjValue);
            bool strictlyBetter = deltaBestViolation > 0 ||
                                  (model.optimiseMode != OptimiseMode::NONE &&
                                   deltaBestViolation == 0 && deltaBestObj > 0);
            if (strictlyBetter) {
                bestViolation = lastViolation;
                if (model.optimiseMode != OptimiseMode::NONE) {
                    bestObjValue = lastObjValue;
                }
                newBestSolution();
            }
            return true;
        }
    }

   public:
    HillClimber(Model model)
        : model(std::move(model)),
          selectionStrategy(this->model.neighbourhoods.size()),
          stats(this->model.neighbourhoods.size()) {}
    void search() {
        triggerEventCount = 0;
        std::cout << "Neighbourhoods (" << model.neighbourhoods.size()
                  << "):\n";
        std::transform(model.neighbourhoods.begin(), model.neighbourhoods.end(),
                       std::ostream_iterator<std::string>(std::cout, "\n"),
                       [](auto& n) -> std::string& { return n.name; });

        stats.startTimer();
        assignRandomValueToVariables();
        evaluateAndStartTriggeringDefinedExpressions();
        model.csp->evaluate();
        model.csp->startTriggering();
        lastViolation = model.csp->violation;
        bestViolation = lastViolation;

        if (model.optimiseMode != OptimiseMode::NONE) {
            model.objective->evaluate();
            lastObjValue = model.objective->view().value;
            bestObjValue = lastObjValue;
            model.objective->startTriggering();
        }
        newBestSolution();
        selectionStrategy.initialise(model);
        AcceptanceCallBack callback = [&]() { return this->acceptValue(); };
        while (!finished()) {
            int nextNeighbourhoodIndex =
                selectionStrategy.nextNeighbourhood(model);
            Neighbourhood& neighbourhood =
                model.neighbourhoods[nextNeighbourhoodIndex];
            debug_code(debug_log("Before neighbourhood application, state is:");
                       newBestSolution());
            debug_neighbourhood_action(
                "Applying neighbourhood: " << neighbourhood.name << ":");
            auto& var =
                model.variables
                    [model.neighbourhoodVarMapping[nextNeighbourhoodIndex]];
            ParentCheckCallBack alwaysTrueFunc(alwaysTrue);
            NeighbourhoodParams params(callback, alwaysTrueFunc, 1, var.second,
                                       stats, model.vioDesc);
            u_int64_t minorNodeCountAtStart = stats.minorNodeCount,
                      triggerEventCountAtStart = triggerEventCount;
            double cpuTimeAtStart = stats.getCpuTime();
            neighbourhood.apply(params);
            NeighbourhoodResult result(
                model, lastViolation, nextNeighbourhoodIndex,
                stats.minorNodeCount - minorNodeCountAtStart,
                triggerEventCount - triggerEventCountAtStart,
                stats.getCpuTime() - cpuTimeAtStart);
            stats.reportResult(result);
            selectionStrategy.reportResult(result);
        }
        stats.endTimer();
        std::cout << stats << "\nTrigger event count " << triggerEventCount
                  << "\n\n";
        printNeighbourhoodStats();
    }

    inline void newBestSolution() {
        violationBackOff = 1;
        numberItersAtLastEvent = stats.numberIterations;
        if (model.optimiseMode != OptimiseMode::NONE && lastViolation == 0) {
            std::cout << "Best solution: " << lastObjValue << " "
                      << stats.getCpuTime() << std::endl;
        }
        std::cout << "Violation " << lastViolation;
        if (model.optimiseMode != OptimiseMode::NONE) {
            std::cout << ", objective " << lastObjValue;
        }
        std::cout << std::endl;
        std::cout << stats << "\nTrigger event count " << triggerEventCount
                  << "\n\n";
        if (lastViolation == 0) {
            std::cout << "solution start\n";
        }
        for (size_t i = 0; i < model.variables.size(); ++i) {
            auto& v = model.variables[i];
            std::cout << "letting " << model.variableNames[i] << " be ";
            if (valBase(v.second).container != &definedPool) {
                prettyPrint(std::cout, v.second);
            } else {
                prettyPrint(std::cout,
                            model.definingExpressions.at(valBase(v.second).id));
            }
            std::cout << std::endl;
        }
        if (lastViolation == 0) {
            std::cout << "solution end\n";
        }
        debug_code(debug_log("CSP state:");
                   model.csp->dumpState(std::cout) << std::endl;
                   if (model.optimiseMode != OptimiseMode::NONE) {
                       debug_log("Objective state:");
                       model.objective->dumpState(std::cout) << std::endl;
                   });
    }

    inline bool finished() {
        if (sigIntActivated) {
            std::cout << "control-c pressed\n";
            return true;
        }
        if (sigAlarmActivated) {
            std::cout << "timeout\n";
            return true;
        }
        if (stats.numberIterations - numberItersAtLastEvent > 120000) {
            lastViolation = bestViolation + violationBackOff;
            std::cout << "Setting last violation to " << lastViolation
                      << std::endl;
            ++violationBackOff;
            otherPhase = true;
            numberItersAtLastEvent = stats.numberIterations;
        }
        return (model.optimiseMode == OptimiseMode::NONE &&
                bestViolation == 0) ||
               stats.numberIterations >= 30000000;
    }

    inline void assignRandomValueToVariables() {
        for (auto& var : model.variables) {
            if (valBase(var.second).container == &definedPool) {
                continue;
            }
            assignRandomValueInDomain(var.first, var.second, stats);
        }
    }

    inline void evaluateAndStartTriggeringDefinedExpressions() {
        for (auto& nameExprPair : model.definingExpressions) {
            mpark::visit(
                [&](auto& expr) {
                    expr->evaluate();
                    expr->startTriggering();
                },
                nameExprPair.second);
        }
    }
    void printNeighbourhoodStats() {
        static const std::string indent = "    ";
        std::cout << "---------------------\n";
        for (size_t i = 0; i < model.neighbourhoods.size(); i++) {
            std::cout << "Neighbourhood: " << model.neighbourhoods[i].name
                      << std::endl;
            std::cout << indent << "value,total,average\n";
            std::cout << indent << "Number activations,"
                      << stats.nhActivationCounts[i] << ","
                      << ((stats.nhActivationCounts[i] == 0) ? 0 : 1)
                      << std::endl;
            std::cout << indent << "Number minor nodes,"
                      << stats.nhMinorNodeCounts[i] << ","
                      << stats.getAverage(stats.nhMinorNodeCounts[i], i)
                      << std::endl;
            std::cout << indent << "Number trigger events,"
                      << stats.nhTriggerEventCounts[i] << ","
                      << stats.getAverage(stats.nhTriggerEventCounts[i], i)
                      << std::endl;
            std::cout << indent << "Time," << stats.nhTotalCpuTimes[i] << ","
                      << stats.getAverage(stats.nhTotalCpuTimes[i], i)
                      << std::endl;
        }
    }
};

void dumpVarViolations(const ViolationContainer& vioDesc) {
    auto sortedVars = vioDesc.getVarsWithViolation();
    std::sort(sortedVars.begin(), sortedVars.end());
    for (auto& var : sortedVars) {
        std::cout << "var: " << var
                  << ", violation=" << vioDesc.varViolation(var) << std::endl;
    }
    for (auto& var : sortedVars) {
        if (vioDesc.hasChildViolation(var)) {
            std::cout << "Entering var " << var << std::endl;
            dumpVarViolations(vioDesc.childViolations(var));
            std::cout << "exiting var " << var << std::endl;
        }
    }
}
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
