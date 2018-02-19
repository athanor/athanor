
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
    u_int64_t violationBackOff = 1;
    bool otherPhase = false;
    u_int64_t bestViolation;
    int64_t lastObjValue;
    int64_t bestObjValue;
    bool newBestValueFound = false;
    StatsContainer stats;
    u_int64_t numberNodesAtLastEvent;

    int64_t getDeltaObj(int64_t oldObj, int64_t newObj) {
        switch (model.optimiseMode) {
            case OptimiseMode::MAXIMISE:
                return newObj - oldObj;
            case OptimiseMode::MINIMISE:
                return oldObj - newObj;
            case OptimiseMode::NONE:
                return 0;
        }
    }
    inline bool acceptValue() {
        u_int64_t newObjValue = getView<IntView>(model.objective).value;
        int64_t deltaObj = getDeltaObj(lastObjValue, newObjValue);
        int64_t deltaViolation = lastViolation - model.csp.violation;
        bool solutionAllowed;
        if (!otherPhase) {
            solutionAllowed =
                deltaViolation > 0 || (deltaViolation == 0 && deltaObj >= 0);
        } else {
            solutionAllowed = deltaViolation >= 0 &&
                              getDeltaObj(bestObjValue, newObjValue) > 0;
            if (solutionAllowed && model.csp.violation == 0) {
                otherPhase = false;
                numberNodesAtLastEvent = stats.majorNodeCount;
            }
        }
        if (!solutionAllowed) {
            return false;
        } else {
            if (deltaViolation > 0 || (deltaViolation == 0 && deltaObj > 0)) {
                numberNodesAtLastEvent = stats.majorNodeCount;
            }

            lastViolation = model.csp.violation;
            if (model.optimiseMode != OptimiseMode::NONE) {
                lastObjValue = newObjValue;
            }
            int64_t deltaBestViolation = bestViolation - lastViolation;
            int64_t deltaBestObj = getDeltaObj(bestObjValue, lastObjValue);
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
        bestViolation = lastViolation;
        startTriggering(model.csp);

        if (model.optimiseMode != OptimiseMode::NONE) {
            evaluate(model.objective);
            lastObjValue = getView<IntView>(model.objective).value;
            bestObjValue = lastObjValue;
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
        violationBackOff = 1;
        numberNodesAtLastEvent = stats.majorNodeCount;
        std::cout << "Violation " << lastViolation;
        if (model.optimiseMode != OptimiseMode::NONE) {
            std::cout << ", objective " << lastObjValue;
        }
        std::cout << std::endl;
        std::cout << stats << std::endl;

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
        if (stats.majorNodeCount - numberNodesAtLastEvent > 40000) {
            lastViolation = bestViolation + violationBackOff;
            std::cout << "Setting last violation to " << lastViolation
                      << std::endl;
            ++violationBackOff;
            otherPhase = true;
            numberNodesAtLastEvent = stats.majorNodeCount;
        }
        return (model.optimiseMode == OptimiseMode::NONE &&
                bestViolation == 0) ||
               stats.majorNodeCount >= 30000000;
    }
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
