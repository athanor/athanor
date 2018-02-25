#ifndef SRC_SEARCH_MODEL_H_
#define SRC_SEARCH_MODEL_H_
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/opAnd.h"
#include "operators/operatorBase.h"
#include "search/violationDescription.h"
#include "types/base.h"
#include "types/int.h"
#include "types/typeOperations.h"

class ModelBuilder;
enum OptimiseMode { NONE, MAXIMISE, MINIMISE };
struct Model {
    friend ModelBuilder;
    std::vector<std::pair<AnyDomainRef, AnyValRef>> variables;
    std::vector<std::string> variableNames;
    std::vector<Neighbourhood> neighbourhoods;
    std::vector<int> neighbourhoodVarMapping;
    std::vector<std::vector<int>> varNeighbourhoodMapping;
    OpAnd csp;
    IntReturning objective;
    OptimiseMode optimiseMode;
    ViolationDescription vioDesc;

   private:
    Model(std::vector<std::pair<AnyDomainRef, AnyValRef>> variables,
          std::vector<std::string> variableNames,
          std::vector<Neighbourhood> neighbourhoods,
          std::vector<int> neighbourhoodVarMapping,
          std::vector<std::vector<int>> varNeighbourhoodMapping,
          std::vector<BoolReturning> constraints, IntReturning objective,
          OptimiseMode optimiseMode)
        : variables(std::move(variables)),
          variableNames(std::move(variableNames)),

          neighbourhoods(std::move(neighbourhoods)),
          neighbourhoodVarMapping(std::move(neighbourhoodVarMapping)),
          varNeighbourhoodMapping(std::move(varNeighbourhoodMapping)),
          csp(std::make_shared<FixedArray<BoolReturning>>(
              std::move(constraints))),
          objective(std::move(objective)),
          optimiseMode(optimiseMode) {}
};

class ModelBuilder {
    std::vector<std::pair<AnyDomainRef, AnyValRef>> variables;
    std::vector<std::string> variableNames;
    std::vector<BoolReturning> constraints;
    IntReturning objective = constructValueFromDomain(
        IntDomain({intBound(0, 0)}));  // non applicable default
                                       // to avoid undefined
                                       // variant
    OptimiseMode optimiseMode = OptimiseMode::NONE;

   public:
    ModelBuilder() {}

    inline void addConstraint(BoolReturning constraint) {
        if (constraintHandledByDefine(constraint)) {
            return;
        } else {
            constraints.emplace_back(std::move(constraint));
        }
    }

    template <typename DomainPtrType,
              typename ValueType = typename AssociatedValueType<
                  typename DomainPtrType::element_type>::type>
    inline ValRef<ValueType> addVariable(std::string name,
                                         const DomainPtrType& domainImpl) {
        variables.emplace_back(AnyDomainRef(domainImpl),
                               AnyValRef(ValRef<ValueType>(nullptr)));
        auto& val = variables.back().second.emplace<ValRef<ValueType>>(
            constructValueFromDomain(*domainImpl));
        valBase(*val).id = variables.size() - 1;
        valBase(*val).container = NULL;
        variableNames.emplace_back(std::move(name));
        return val;
    }

    inline void setObjective(OptimiseMode mode, IntReturning obj) {
        objective = std::move(obj);
        optimiseMode = mode;
        ;
    }
    Model build() {
        std::vector<Neighbourhood> neighbourhoods;
        std::vector<int> neighbourhoodVarMapping;
        std::vector<std::vector<int>> varNeighbourhoodMapping;
        for (size_t i = 0; i < variables.size(); ++i) {
            if (mpark::visit(
                    [](auto& val) { return val->container == &constantPool; },
                    variables[i].second)) {
                continue;
            }
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
        return Model(std::move(variables), std::move(variableNames),
                     std::move(neighbourhoods),
                     std::move(neighbourhoodVarMapping),
                     std::move(varNeighbourhoodMapping), std::move(constraints),
                     std::move(objective), optimiseMode);
    }
    template <typename T, typename Variant>
    inline ValRef<T>* asValRef(Variant& v) {
        return mpark::get_if<ValRef<T>>(&v);
    }

    inline bool constraintHandledByDefine(BoolReturning& constraint) {
        return mpark::visit(
            overloaded(
                [&](std::shared_ptr<OpIntEq>& op) {
                    auto l = asValRef<IntValue>(op->left);
                    auto r = asValRef<IntValue>(op->right);
                    if (l && (*l)->container != &constantPool) {
                        define(*l, op->right);
                        return true;
                    } else if (r && (*r)->container != &constantPool) {
                        define(*r, op->left);
                        return true;
                    }
                    return false;
                },
                [&](auto&) { return false; }),
            constraint);
    }

    template <typename ValueType, typename ReturningType = typename ReturnType<
                                      ValRef<ValueType>>::type>
    void define(ValRef<ValueType> val, ReturningType expr) {
        addTrigger(expr, std::make_shared<DefinedTrigger<ValueType>>(val));
        val->container = &constantPool;
    }
};
#endif /* SRC_SEARCH_MODEL_H_ */
