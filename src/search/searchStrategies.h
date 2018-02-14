
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
    u_int64_t lastViolation;
    u_int64_t bestViolation;
    int64_t allowedViolationDelta = 0;
    int64_t bestObjValue;
    bool newBestValueFound = false;
    StatsContainer stats;
    u_int64_t numberNodesAtLastSolution;
    inline bool acceptValue() {
        u_int64_t newObjValue = getView<IntView>(model.objective).value;
        int64_t deltaObj;

        switch (model.optimiseMode) {
            case OptimiseMode::MAXIMISE:
                deltaObj = newObjValue - bestObjValue;
                break;
            case OptimiseMode::MINIMISE:
                deltaObj = bestObjValue - newObjValue;
                break;
            case OptimiseMode::NONE:
                deltaObj = 0;
                break;
        }
        int64_t deltaViolation = lastViolation - model.csp.violation;
        bool solutionAllowed =
            deltaViolation + allowedViolationDelta > 0 ||
            (deltaViolation + allowedViolationDelta == 0 && deltaObj >= 0);
        if (!solutionAllowed) {
            return false;
        } else {
            lastViolation = model.csp.violation;
            if (lastViolation < bestViolation) {
                bestViolation = lastViolation;
            }
            if (model.optimiseMode != OptimiseMode::NONE) {
                bestObjValue = newObjValue;
            }
            int64_t deltaBestViolation = bestViolation - lastViolation;
            bool strictlyBetter = deltaBestViolation > 0 ||
                                  (deltaBestViolation == 0 && deltaObj > 0);
            if (strictlyBetter) {
                newBestSolution();
            }
            return true;
        }
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
        lastViolation = model.csp.violation;
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
                NeighbourhoodResult(model, lastViolation));
        }
        stats.endTimer();
        std::cout << stats << std::endl;
    }

    inline void newBestSolution() {
        allowedViolationDelta = 0;
        numberNodesAtLastSolution = stats.majorNodeCount;
        std::cout << "Violation " << lastViolation << ", objective "
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
        if (stats.majorNodeCount - numberNodesAtLastSolution > 1000000) {
            allowedViolationDelta += 1;
        }
        return (model.optimiseMode == OptimiseMode::NONE &&
                lastViolation == 0) ||
               stats.majorNodeCount >= 30000000;
    }
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
