#ifndef SRC_SEARCH_MODEL_H_
#define SRC_SEARCH_MODEL_H_
#include <algorithm>
#include "operators/operatorMakers.h"

#include <cassert>
#include "base/base.h"
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/opAnd.h"
#include "operators/opIntEq.h"
#include "operators/opSequenceLit.h"
#include "search/violationDescription.h"
#include "types/allTypes.h"
extern ValBase definedPool;
class ModelBuilder;
enum OptimiseMode { NONE, MAXIMISE, MINIMISE };
struct Model {
    friend ModelBuilder;
    std::vector<std::pair<AnyDomainRef, AnyValRef>> variables;
    std::vector<std::string> variableNames;
    std::vector<Neighbourhood> neighbourhoods;
    std::vector<int> neighbourhoodVarMapping;
    std::vector<std::vector<int>> varNeighbourhoodMapping;
    std::shared_ptr<OpAnd> csp = nullptr;
    ExprRef<IntView> objective = make<IntValue>().asExpr();
    OptimiseMode optimiseMode = OptimiseMode::NONE;
    ViolationDescription vioDesc;
    std::unordered_map<size_t, AnyExprRef> definingExpressions;

   private:
    Model() {}
};

class ModelBuilder {
    Model model;
    ExprRefVec<BoolView> constraints;
    std::vector<AnyValRef> varsToBeDefined;

   public:
    ModelBuilder() {}

    inline void addConstraint(ExprRef<BoolView> constraint) {
        if (!constraintHandledByDefine(constraint)) {
            constraints.emplace_back(std::move(constraint));
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
        if (model.optimiseMode != OptimiseMode::NONE) {
            addConstraint(OpMaker<OpIsDefined>::make(model.objective));
        }
        model.csp = std::make_shared<OpAnd>(
            std::make_shared<OpSequenceLit>(move(constraints)));
        for (size_t i = 0; i < model.variables.size(); ++i) {
            if (valBase(model.variables[i].second).container == &definedPool) {
                model.varNeighbourhoodMapping.emplace_back();
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
        handleVarsToBeDefined();
        ExprRef<BoolView> cspExpr(model.csp);
        model.csp->optimise(PathExtension::begin(cspExpr));
        if (model.optimiseMode != OptimiseMode::NONE) {
            model.objective->optimise(PathExtension::begin(model.objective));
        }
        return std::move(model);
    }
    template <typename View,
              typename Value = typename AssociatedValueType<View>::type>
    inline ValRef<Value> getIfNonConstValue(ExprRef<View>& expr) {
        Value* value = dynamic_cast<Value*>(&(*expr));
        if (value && value->container != &constantPool) {
            return assumeAsValue(expr);
        } else {
            return ValRef<Value>(nullptr);
        }
    }

    inline bool constraintHandledByDefine(ExprRef<BoolView>& constraint) {
        BoolView* eqExprTester = &(constraint->view());
        OpIntEq* opIntEq;
        if ((opIntEq = dynamic_cast<OpIntEq*>(eqExprTester)) != NULL) {
            ValRef<IntValue> definedVar = getIfNonConstValue(opIntEq->left);
            if (definedVar && valBase(definedVar).container != &definedPool) {
                define(definedVar, opIntEq->right);
                return true;
            }
            definedVar = getIfNonConstValue(opIntEq->right);
            if (definedVar && valBase(definedVar).container != &definedPool) {
                define(definedVar, opIntEq->left);
                return true;
            }
        }
        return false;
    }

    template <typename Value,
              typename View = typename AssociatedViewType<Value>::type>
    void define(ValRef<Value>& val, ExprRef<View>& expr) {
        valBase(*val).container = &definedPool;
        varsToBeDefined.emplace_back(val);
        model.definingExpressions.emplace(valBase(val).id, expr);
    }

    inline void handleVarsToBeDefined() {
        for (auto& var : varsToBeDefined) {
            auto func = makeFindReplaceFunc(
                var, model.definingExpressions.at(valBase(var).id));
            model.csp->operand = findAndReplace(model.csp->operand, func);
            model.objective = findAndReplace(model.objective, func);
        }
    }

    inline FindAndReplaceFunction makeFindReplaceFunc(AnyValRef& var,
                                                      AnyExprRef& expr) {
        return mpark::visit(
            [&](auto& var) -> FindAndReplaceFunction {
                typedef
                    typename AssociatedViewType<valType(var)>::type ViewType;
                auto& exprImpl = mpark::get<ExprRef<ViewType>>(expr);
                return [this, var, exprImpl](
                           AnyExprRef ref) -> std::pair<bool, AnyExprRef> {
                    auto exprRefTest = mpark::get_if<ExprRef<ViewType>>(&ref);
                    if (exprRefTest) {
                        auto valRefTest =
                            this->getIfNonConstValue(*exprRefTest);
                        if (valRefTest &&
                            (valBase(valRefTest) == valBase(var))) {
                            return std::make_pair(true, exprImpl);
                        }
                    }
                    return std::make_pair(false, ref);
                };
            },
            var);
    }
};

#endif /* SRC_SEARCH_MODEL_H_ */
