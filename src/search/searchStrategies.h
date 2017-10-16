
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <cassert>
#include "search/model.h"
#include "types/forwardDecls/print.h"

#include "search/neighbourhoodSelectionStrategies.h"
template <typename NeighbourhoodSelectionStrategy>
class HillClimber {
    Model model;
    NeighbourhoodSelectionStrategy selectionStrategy;

   public:
    HillClimber(Model model)
        : model(std::move(model)),
          selectionStrategy(this->model.neighbourhoods.size()) {}
    void search() {
        BoolView cspView = getBoolView(model.csp);
        for (auto& var : model.variables) {
            assignRandomValueInDomain(var.first, var.second);
        }
        evaluate(model.csp);
        startTriggering(model.csp);
        std::cout << "After random initialisation, violation is "
                  << cspView.violation << std::endl;
        std::cout << "Values are:\n";
        for (auto& var : model.variables) {
            prettyPrint(std::cout, var.second) << std::endl;
        }
        u_int64_t bestViolation = cspView.violation;
        AcceptanceCallBack callback = [&]() {
            return cspView.violation <= bestViolation;
        };
        while (cspView.violation != 0) {
            int nextNeighbourhoodIndex = selectionStrategy.nextNeighbourhood();
            Neighbourhood& neighbourhood =
                model.neighbourhoods[nextNeighbourhoodIndex];
            auto& var =
                model.variables
                    [model.neighbourhoodVarMapping[nextNeighbourhoodIndex]];
            auto& varBackup =
                model.variablesBackup
                    [model.neighbourhoodVarMapping[nextNeighbourhoodIndex]];
            prettyPrint(std::cout << "Trying neighbourhood: "
                                  << neighbourhood.name << " on variable ",
                        var.second)
                << std::endl;

            neighbourhood.apply(callback, varBackup.second, var.second);
            if (cspView.violation < bestViolation) {
                bestViolation = cspView.violation;
                std::cout << "Better value found, violation is "
                          << cspView.violation << std::endl;
                std::cout << "Values are:\n";
                for (auto& var : model.variables) {
                    prettyPrint(std::cout, var.second) << std::endl;
                }
            }
        }
    }
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
