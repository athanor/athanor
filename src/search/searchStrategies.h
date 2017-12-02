
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <cassert>
#include "operators/operatorBase.h"
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "types/typeOperations.h"

template <typename NeighbourhoodSelectionStrategy>
class HillClimber {
    Model model;
    NeighbourhoodSelectionStrategy selectionStrategy;

   public:
    HillClimber(Model model)
        : model(std::move(model)),
          selectionStrategy(this->model.neighbourhoods.size()) {}
    void search() {
        BoolView& cspView = model.csp;
        for (auto& var : model.variables) {
            assignRandomValueInDomain(var.first, var.second);
        }
        evaluate(model.csp);
        startTriggering(model.csp);
        u_int64_t bestViolation = cspView.violation;
        newBestSolution(bestViolation);
        selectionStrategy.initialise(model);
        AcceptanceCallBack callback = [&]() {
            return cspView.violation <= bestViolation;
        };
        size_t iterationCount = 0;
        while (cspView.violation != 0 && iterationCount++ < 3000000) {
            int nextNeighbourhoodIndex =
                selectionStrategy.nextNeighbourhood(model);
            Neighbourhood& neighbourhood =
                model.neighbourhoods[nextNeighbourhoodIndex];
            auto& var =
                model.variables
                    [model.neighbourhoodVarMapping[nextNeighbourhoodIndex]];
            if (false && bestViolation <= 2) {
                std::cout << "trying neighbourhood " << neighbourhood.name
                          << std::endl;
                prettyPrint(std::cout << "on var : ", var.second) << std::endl;
            }
            auto& varBackup =
                model.variablesBackup
                    [model.neighbourhoodVarMapping[nextNeighbourhoodIndex]];
            neighbourhood.apply(callback, varBackup.second, var.second);
            if (cspView.violation < bestViolation) {
                bestViolation = cspView.violation;
                newBestSolution(bestViolation);
            }
            selectionStrategy.reportResult(
                NeighbourhoodResult(model, bestViolation));
        }
    }

    inline void newBestSolution(u_int64_t bestViolation) {
        std::cout << "best v " << bestViolation << std::endl;
        for (size_t i = 0; i < model.variables.size(); ++i) {
            model.variablesBackup[i].second =
                deepCopy(model.variables[i].second);
            normalise(model.variablesBackup[i].second);
        }
        std::sort(
            model.variablesBackup.begin(), model.variablesBackup.end(),
            [](auto& u, auto& v) { return smallerValue(u.second, v.second); });
        std::cout << "new best violation: " << bestViolation << std::endl;
        for (auto& v : model.variablesBackup) {
            prettyPrint(std::cout << "var: ", v.second) << std::endl;
        }
    }
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
