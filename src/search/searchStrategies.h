
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <cassert>
#include "operators/operatorBase.h"
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/statsContainer.h"
#include "types/typeOperations.h"

inline bool alwaysTrue(const AnyValRef&) { return true; }

template <typename NeighbourhoodSelectionStrategy>
class HillClimber {
    Model model;
    NeighbourhoodSelectionStrategy selectionStrategy;
    u_int64_t bestViolation;
    int64_t bestObjValue;
    bool newBestValueFound = false;
    StatsContainer stats;

    inline bool acceptValue() {
        bool objAtLeastAsGood;
        int64_t newObjValue = objAtLeastAsGood =
            getView<IntView>(model.objective).value;
        switch (model.optimiseMode) {
            case OptimiseMode::MAXIMISE:
                objAtLeastAsGood = newObjValue >= bestObjValue;
                break;
            case OptimiseMode::MINIMISE:
                objAtLeastAsGood = newObjValue <= bestObjValue;
                break;
            case OptimiseMode::NONE:
                objAtLeastAsGood = false;
                break;
        }
        if (objAtLeastAsGood) {
            bestObjValue = newObjValue;
        }
        bool violationBetter = model.csp.violation < bestViolation;
        if (violationBetter) {
            bestViolation = model.csp.violation;
        }
        newBestValueFound =
            violationBetter ||
            (model.csp.violation == bestViolation &&
             (model.optimiseMode == OptimiseMode::NONE || objAtLeastAsGood));
        if (newBestValueFound) {
            newBestSolution();
        }
        return newBestValueFound;
    }

   public:
    HillClimber(Model model)
        : model(std::move(model)),
          selectionStrategy(this->model.neighbourhoods.size()) {}
    void search() {
        std::cout << "Neighbourhoods (" << model.neighbourhoods.size()
                  << "):\n";
        std::transform(model.neighbourhoods.begin(), model.neighbourhoods.end(),
                       std::ostream_iterator<std::string>(std::cout, "\n"),
                       [](auto& n) -> std::string& { return n.name; });

        stats.startTimer();
        for (auto& var : model.variables) {
            assignRandomValueInDomain(var.first, var.second);
        }
        evaluate(model.csp);
        bestViolation = model.csp.violation;
        startTriggering(model.csp);

        if (model.optimiseMode != OptimiseMode::NONE) {
            evaluate(model.objective);
            bestObjValue = getView<IntView>(model.objective).value;
            startTriggering(model.objective);
        }
        newBestSolution();
        selectionStrategy.initialise(model);
        AcceptanceCallBack callback = [&]() { return this->acceptValue(); };
        while (!finished()) {
            ++stats.majorNodeCount;
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
            NeighbourhoodParams params(callback, alwaysTrue, 1, var.second,
                                       stats, model.vioDesc);
            neighbourhood.apply(params);
            selectionStrategy.reportResult(
                NeighbourhoodResult(model, bestViolation));
        }
        stats.endTimer();
        std::cout << stats << std::endl;
    }

    inline void newBestSolution() {
        std::cout << "Violation " << bestViolation << ", objective "
                  << bestObjValue << std::endl;

        for (auto& v : model.variables) {
            prettyPrint(std::cout << "var: ", v.second) << std::endl;
        }
        debug_code(debug_log("CSP state:");
                   dumpState(std::cout, model.csp) << std::endl;
                   if (model.optimiseMode != OptimiseMode::NONE) {
                       debug_log("Objective state:");
                       dumpState(std::cout, model.objective) << std::endl;
                   });
    }

    inline bool finished() {
        return (model.optimiseMode == OptimiseMode::NONE &&
                bestViolation == 0) ||
               stats.majorNodeCount >= 3000000;
    }
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
