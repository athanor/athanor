#ifndef SRC_SEARCH_MODEL_H_
#define SRC_SEARCH_MODEL_H_
#include <algorithm>
#include "neighbourhoods/neighbourhoods.h"
#include "operators/boolOperators.h"
#include "operators/boolProducing.h"
#include "operators/intProducing.h"
#include "types/forwardDecls/copy.h"
#include "types/forwardDecls/typesAndDomains.h"
class ModelBuilder;
enum OptimiseMode { NONE, MAXIMISE, MINIMISE };
struct Model {
    friend ModelBuilder;
    std::vector<std::pair<Domain, Value>> variables;
    std::vector<std::pair<Domain, Value>> variablesBackup;  // not used at the
                                                            // moment, just
                                                            // needed to invoke
                                                            // neighbourhoods
    std::vector<Neighbourhood> neighbourhoods;
    std::vector<int> neighbourhoodVarMapping;
    OpAnd csp;
    IntProducing objective;
    OptimiseMode optimiseMode;

   private:
    Model(std::vector<std::pair<Domain, Value>> variables,
          std::vector<std::pair<Domain, Value>> variablesBackup,
          std::vector<Neighbourhood> neighbourhoods,
          std::vector<int> neighbourhoodVarMapping,
          std::vector<BoolProducing> constraints, IntProducing objective,
          OptimiseMode optimiseMode)
        : variables(std::move(variables)),
          variablesBackup(std::move(variablesBackup)),
          neighbourhoods(std::move(neighbourhoods)),
          neighbourhoodVarMapping(std::move(neighbourhoodVarMapping)),
          csp(std::move(constraints)),
          objective(std::move(objective)),
          optimiseMode(optimiseMode) {}
};

class ModelBuilder {
    std::vector<std::pair<Domain, Value>> variables;
    std::vector<BoolProducing> constraints;
    IntProducing objective = construct<IntValue>();  // non applicable default
                                                     // to avoid undefined
                                                     // variant
    OptimiseMode optimiseMode = OptimiseMode::NONE;

   public:
    ModelBuilder() {}

    inline void addConstraint(BoolProducing constraint) {
        constraints.emplace_back(std::move(constraint));
    }
    template <typename ValueType>
    inline ValRef<ValueType> addVariable(Domain d) {
        variables.emplace_back(d, Value(ValRef<ValueType>(nullptr)));
        auto& val = variables.back().second.emplace<ValRef<ValueType>>(
            construct<ValueType>());
        typedef std::shared_ptr<typename AssociatedDomain<ValueType>::type>
            DomainPtrType;
        auto& domainImpl = mpark::get<DomainPtrType>(d);
        matchInnerType(*domainImpl, *val);
        return val;
    }
    inline void setObjective(OptimiseMode mode, IntProducing obj) {
        objective = std::move(obj);
        optimiseMode = mode;
        ;
    }
    Model build() {
        std::vector<Neighbourhood> neighbourhoods;
        std::vector<int> neighbourhoodVarMapping;
        for (size_t i = 0; i < variables.size(); ++i) {
            auto& domain = variables[i].first;
            size_t previousNumberNeighbourhoods = neighbourhoods.size();
            generateNeighbourhoods(domain, neighbourhoods);
            neighbourhoodVarMapping.insert(
                neighbourhoodVarMapping.end(),
                neighbourhoods.size() - previousNumberNeighbourhoods, i);
        }
        std::vector<std::pair<Domain, Value>> variablesBackup;
        std::transform(variables.begin(), variables.end(),
                       std::back_inserter(variablesBackup), [](auto& var) {
                           return std::make_pair(var.first,
                                                 deepCopyValue(var.second));
                       });
        return Model(std::move(variables), std::move(variablesBackup),
                     std::move(neighbourhoods),
                     std::move(neighbourhoodVarMapping), std::move(constraints),
                     std::move(objective), optimiseMode);
    }
};
#endif /* SRC_SEARCH_MODEL_H_ */
