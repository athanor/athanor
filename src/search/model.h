#ifndef SRC_SEARCH_MODEL_H_
#define SRC_SEARCH_MODEL_H_
#include <algorithm>
#include <cassert>
#include <chrono>
#include "base/base.h"
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/opAnd.h"
#include "operators/opIntEq.h"
#include "operators/opSequenceLit.h"
#include "operators/operatorMakers.h"
#include "search/objective.h"
#include "search/violationContainer.h"
#include "types/allVals.h"

extern ValBase definedPool;
class ModelBuilder;

struct Model {
    friend ModelBuilder;
    std::vector<std::pair<AnyDomainRef, AnyValRef>> variables;
    std::vector<std::string> variableNames;
    std::vector<Neighbourhood> neighbourhoods;
    std::vector<int> neighbourhoodVarMapping;
    std::vector<std::vector<int>> varNeighbourhoodMapping;
    std::shared_ptr<OpAnd> csp = nullptr;
    AnyExprRef objective = make<IntValue>().asExpr();
    OptimiseMode optimiseMode = OptimiseMode::NONE;
    HashMap<size_t, AnyExprRef> definingExpressions;

   private:
    Model() { mpark::get<ExprRef<IntView>>(objective)->view()->value = 0; }

   public:
    void printVariables() const;
    void debugSanityCheck() const;

   public:
    inline UInt getViolation() const { return csp->view()->violation; }
    Objective getObjective() const;
    bool objectiveDefined() const;
};

class ModelBuilder {
    Model model;
    ExprRefVec<BoolView> constraints;
    std::vector<AnyValRef> varsToBeDefined;

    void createNeighbourhoods();
    void addConstraints();
    void substituteVarsToBeDefined();
    FindAndReplaceFunction makeFindReplaceFunc(AnyValRef& var,
                                               AnyExprRef& expr);

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

    inline void setObjective(OptimiseMode mode, AnyExprRef obj) {
        model.objective = std::move(obj);
        model.optimiseMode = mode;
    }
    Model build();
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
        BoolView* eqExprTester = &(constraint->view().get());
        OpIntEq* opIntEq = dynamic_cast<OpIntEq*>(eqExprTester);
        if (opIntEq != NULL) {
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
};

#endif /* SRC_SEARCH_MODEL_H_ */
