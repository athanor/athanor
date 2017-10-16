
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
        assert(model.optimiseMode != OptimiseMode::NONE);
        IntView objView = getIntView(model.objective);
        for (auto& var : model.variables) {
            assignRandomValueInDomain(var.first, var.second);
        }
        evaluate(model.objective);
        startTriggering(model.objective);
        std::cout << "After random initialisation, objective is "
                  << objView.value << std::endl;
        std::cout << "Values are:\n";
        for (auto& var : model.variables) {
            prettyPrint(std::cout, var.second) << std::endl;
        }
        int bestObj = objView.value;
        AcceptanceCallBack callback = [&]() {
            return objView.value >= bestObj;
        };
        while (true) {
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
            if (objView.value > bestObj) {
                bestObj = objView.value;
                std::cout << "Better value found, objective is "
                          << objView.value << std::endl;
                std::cout << "Values are:\n";
                for (auto& var : model.variables) {
                    prettyPrint(std::cout, var.second) << std::endl;
                }
            }
        }
    }
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
