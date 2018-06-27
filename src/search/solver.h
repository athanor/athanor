
#ifndef SRC_SEARCH_SOLVER_H_
#define SRC_SEARCH_SOLVER_H_
#include <cassert>
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/searchStrategies.h"
#include "search/statsContainer.h"
#include "types/allTypes.h"
void dumpVarViolations(const ViolationContainer& vioDesc);
extern bool sigIntActivated, sigAlarmActivated;
extern u_int64_t iterationLimit;
inline bool alwaysTrue(const AnyValVec&) { return true; }

template <typename SearchStrategy, typename NeighbourhoodSelectionStrategy>
class Solver {
    Model model;
    SearchStrategy searchStrategy;
    NeighbourhoodSelectionStrategy selectionStrategy;
    StatsContainer stats;

   public:
    Solver(Model model)
        : model(std::move(model)),
          searchStrategy(this->model),
          selectionStrategy(this->model.neighbourhoods.size()),
          stats(this->model) {}

    auto makeVecFrom(AnyValRef& val) {
        return mpark::visit(
            [](auto& val) -> AnyValVec {
                ValRefVec<valType(val)> vec;
                vec.emplace_back(val);
                return vec;
            },
            val);
    }
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

        model.objective->evaluate();
        model.objective->startTriggering();

        stats.initialSolution(model);

        selectionStrategy.initialise(model);

        while (!finished()) {
            int nextNeighbourhoodIndex =
                selectionStrategy.nextNeighbourhood(model);
            Neighbourhood& neighbourhood =
                model.neighbourhoods[nextNeighbourhoodIndex];
            debug_code(debug_log("Before neighbourhood application, state is:");
                       stats.printCurrentState(model));
            debug_neighbourhood_action(
                "Applying neighbourhood: " << neighbourhood.name << ":");
            auto& var =
                model.variables
                    [model.neighbourhoodVarMapping[nextNeighbourhoodIndex]];
            auto statsMarkPoint = stats.getMarkPoint();

            AcceptanceCallBack callback = [&]() {
                return searchStrategy.acceptSolution(NeighbourhoodResult(
                    model, nextNeighbourhoodIndex, statsMarkPoint));
            };
            ParentCheckCallBack alwaysTrueFunc(alwaysTrue);
            auto changingVariables = makeVecFrom(var.second);
            NeighbourhoodParams params(callback, alwaysTrueFunc, 1,
                                       changingVariables, stats, model.vioDesc);
                        neighbourhood.apply(params);
            NeighbourhoodResult result(model, nextNeighbourhoodIndex,
                                       statsMarkPoint);
            stats.reportResult(result);
            selectionStrategy.reportResult(result);
        }
        stats.endTimer();
        std::cout << stats << "\nTrigger event count " << triggerEventCount
                  << "\n\n";
        printNeighbourhoodStats();
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
        if (iterationLimit != 0 && stats.numberIterations >= iterationLimit) {
            std::cout << "iteration limit reached\n";
            return true;
        }
        return (model.optimiseMode == OptimiseMode::NONE &&
                stats.bestViolation == 0);
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
#endif /* SRC_SEARCH_SOLVER_H_ */
