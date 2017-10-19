#ifndef SRC_SEARCH_MODEL_H_
#define SRC_SEARCH_MODEL_H_
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/boolOperators.h"
#include "operators/boolProducing.h"
#include "operators/intProducing.h"
#include "types/forwardDecls/copy.h"
#include "types/forwardDecls/typesAndDomains.h"
#include "types/int.h"
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
    std::vector<std::vector<int>> varNeighbourhoodMapping;
    OpAnd csp;
    IntProducing objective;
    OptimiseMode optimiseMode;

   private:
    Model(std::vector<std::pair<Domain, Value>> variables,
          std::vector<std::pair<Domain, Value>> variablesBackup,
          std::vector<Neighbourhood> neighbourhoods,
          std::vector<int> neighbourhoodVarMapping,
          std::vector<std::vector<int>> varNeighbourhoodMapping,
          std::vector<BoolProducing> constraints, IntProducing objective,
          OptimiseMode optimiseMode)
        : variables(std::move(variables)),
          variablesBackup(std::move(variablesBackup)),
          neighbourhoods(std::move(neighbourhoods)),
          neighbourhoodVarMapping(std::move(neighbourhoodVarMapping)),
          varNeighbourhoodMapping(std::move(varNeighbourhoodMapping)),
          csp(std::move(constraints)),
          objective(std::move(objective)),
          optimiseMode(optimiseMode) {}
};

class ModelBuilder {
    std::vector<std::pair<Domain, Value>> variables;
    std::vector<BoolProducing> constraints;
    IntProducing objective = constructValueFromDomain(
        IntDomain({intBound(0, 0)}));  // non applicable default
                                       // to avoid undefined
                                       // variant
    OptimiseMode optimiseMode = OptimiseMode::NONE;

   public:
    ModelBuilder() {}

    inline void addConstraint(BoolProducing constraint) {
        constraints.emplace_back(std::move(constraint));
    }
    template <typename DomainPtrType,
              typename ValueType = typename AssociatedValueType<
                  typename DomainPtrType::element_type>::type>
    inline ValRef<ValueType> addVariable(const DomainPtrType& domainImpl) {
        variables.emplace_back(Domain(domainImpl),
                               Value(ValRef<ValueType>(nullptr)));
        auto& val = variables.back().second.emplace<ValRef<ValueType>>(
            constructValueFromDomain(*domainImpl));
        setId(*val, variables.size() - 1);
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
        std::vector<std::vector<int>> varNeighbourhoodMapping;
        for (size_t i = 0; i < variables.size(); ++i) {
            auto& domain = variables[i].first;
            size_t previousNumberNeighbourhoods = neighbourhoods.size();
            generateNeighbourhoods(domain, neighbourhoods);
            neighbourhoodVarMapping.insert(
                neighbourhoodVarMapping.end(),
                neighbourhoods.size() - previousNumberNeighbourhoods, i);
            varNeighbourhoodMapping.emplace_back(neighbourhoods.size() -
                                                 previousNumberNeighbourhoods);
            std::iota(varNeighbourhoodMapping.back().begin(),
                      varNeighbourhoodMapping.back().end(),
                      previousNumberNeighbourhoods);
        }
        assert(neighbourhoods.size() > 0);
        std::vector<std::pair<Domain, Value>> variablesBackup;
        std::transform(variables.begin(), variables.end(),
                       std::back_inserter(variablesBackup), [](auto& var) {
                           return std::make_pair(var.first,
                                                 deepCopyValue(var.second));
                       });
        return Model(std::move(variables), std::move(variablesBackup),
                     std::move(neighbourhoods),
                     std::move(neighbourhoodVarMapping),
                     std::move(varNeighbourhoodMapping), std::move(constraints),
                     std::move(objective), optimiseMode);
    }
};
#endif /* SRC_SEARCH_MODEL_H_ */
