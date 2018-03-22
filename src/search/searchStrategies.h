
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include <cassert>
#include "search/model.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/statsContainer.h"
#include "types/allTypes.h"

extern bool sigIntActivated, sigAlarmActivated;
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
            default:
                assert(false);
                abort();
        }
    }
    inline bool acceptValue() {
        u_int64_t newObjValue = model.objective->value;
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
        assignRandomValueToVariables();
        evaluateDefinedVariables();
        model.csp.evaluate();
        lastViolation = model.csp.violation;
        bestViolation = lastViolation;
        startTriggeringOnDefinedVariables();
        model.csp.startTriggering();

        if (model.optimiseMode != OptimiseMode::NONE) {
            model.objective->evaluate();
            lastObjValue = model.objective->value;
            bestObjValue = lastObjValue;
            model.objective->startTriggering();
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
            ParentCheckCallBack alwaysTrueFunc(alwaysTrue);
            NeighbourhoodParams params(callback, alwaysTrueFunc, 1, var.second,
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
        if (model.optimiseMode != OptimiseMode::NONE && lastViolation == 0) {
            std::cout << "Best solution: " << lastObjValue << " "
                      << stats.getCpuTime() << std::endl;
        }
        std::cout << "Violation " << lastViolation;
        if (model.optimiseMode != OptimiseMode::NONE) {
            std::cout << ", objective " << lastObjValue;
        }
        std::cout << std::endl;
        std::cout << stats << std::endl;

        for (size_t i = 0; i < model.variables.size(); ++i) {
            auto& v = model.variables[i];
            prettyPrint(std::cout << "var: " << model.variableNames[i] << ": ",
                        v.second)
                << std::endl;
        }
        debug_code(debug_log("CSP state:");
                   model.csp.dumpState(std::cout) << std::endl;
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

    inline void assignRandomValueToVariables() {
        for (auto& var : model.variables) {
            if (valBase(var.second).container == &constantPool) {
                continue;
            }
            assignRandomValueInDomain(var.first, var.second);
        }
    }
    inline void evaluateDefinedVariables() {
        for (auto& varExprPair : model.definedMappings) {
            mpark::visit(
                [&](auto& expr) {
                    typedef typename AssociatedValueType<viewType(expr)>::type
                        ValueType;
                    auto& val =
                        mpark::get<ValRef<ValueType>>(varExprPair.first);
                    expr->evaluate();
                    val->initFrom(*expr);
                },
                varExprPair.second);
        }
    }
    inline void startTriggeringOnDefinedVariables() {
        for (auto& varExprPair : model.definedMappings) {
            mpark::visit([&](auto& expr) { expr->startTriggering(); },
                         varExprPair.second);
        }
    }
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
