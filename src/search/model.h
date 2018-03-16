#ifndef SRC_SEARCH_MODEL_H_
#define SRC_SEARCH_MODEL_H_
#include <algorithm>
#include <cassert>
#include "base/base.h"
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/opAnd.h"
#include "search/violationDescription.h"
#include "types/int.h"

class ModelBuilder;
enum OptimiseMode { NONE, MAXIMISE, MINIMISE };
struct Model {
    friend ModelBuilder;
    std::vector<std::pair<AnyDomainRef, AnyValRef>> variables;
    std::vector<std::string> variableNames;
    std::vector<Neighbourhood> neighbourhoods;
    std::vector<int> neighbourhoodVarMapping;
    std::vector<std::vector<int>> varNeighbourhoodMapping;
    OpAnd csp = OpAnd(std::make_shared<FixedArray<BoolView>>(
        std::vector<ExprRef<BoolView>>()));
    ExprRef<IntView> objective = getViewPtr(make<IntValue>());
    OptimiseMode optimiseMode = OptimiseMode::NONE;
    ;
    ViolationDescription vioDesc;
    std::vector<std::pair<AnyValRef, AnyExprRef>> definedMappings;

   private:
    Model() {}
};

class ModelBuilder {
    Model model;

   public:
    ModelBuilder() {}

    inline void addConstraint(ExprRef<BoolView> constraint) {
        auto result = constraint->tryReplaceConstraintWithDefine();
        if (result.first) {
            model.definedMappings.emplace_back(std::move(result.second));
        } else {
            model.csp.quantifier->exprs.emplace_back(std::move(constraint));
        }
    }

    template <typename DomainPtrType,
              typename ValueType = typename AssociatedValueType<
                  typename DomainPtrType::element_type>::type>
    inline ValRef<ValueType> addVariable(std::string name,
                                         const DomainPtrType& domainImpl) {
        model.variables.emplace_back(AnyDomainRef(domainImpl),
                                     AnyValRef(ValRef<ValueType>(nullptr)));
        auto& val = model.variables.back().second.emplace<ValRef<ValueType>>(
            constructValueFromDomain(*domainImpl));
        valBase(*val).id = model.variables.size() - 1;
        valBase(*val).container = NULL;
        model.variableNames.emplace_back(std::move(name));
        return val;
    }

    inline void setObjective(OptimiseMode mode, ExprRef<IntView> obj) {
        model.objective = std::move(obj);
        model.optimiseMode = mode;
    }
    Model build() {
        for (size_t i = 0; i < model.variables.size(); ++i) {
            if (valBase(model.variables[i].second).container == &constantPool) {
                continue;
            }
            auto& domain = model.variables[i].first;
            size_t previousNumberNeighbourhoods = model.neighbourhoods.size();
            generateNeighbourhoods(domain, model.neighbourhoods);
            model.neighbourhoodVarMapping.insert(
                model.neighbourhoodVarMapping.end(),
                model.neighbourhoods.size() - previousNumberNeighbourhoods, i);
            model.varNeighbourhoodMapping.emplace_back(
                model.neighbourhoods.size() - previousNumberNeighbourhoods);
            std::iota(model.varNeighbourhoodMapping.back().begin(),
                      model.varNeighbourhoodMapping.back().end(),
                      previousNumberNeighbourhoods);
        }
        assert(model.neighbourhoods.size() > 0);
        return std::move(model);
    }
};
#endif /* SRC_SEARCH_MODEL_H_ */
