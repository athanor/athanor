#include "search/model.h"
#include <iostream>
#include "search/endOfSearchException.h"
using namespace std;
void ModelBuilder::createNeighbourhoods() {
    for (size_t i = 0; i < model.variables.size(); ++i) {
        if (valBase(model.variables[i].second).container == &definedPool) {
            model.varNeighbourhoodMapping.emplace_back();
            continue;
        }
        auto& domain = model.variables[i].first;
        size_t previousNumberNeighbourhoods = model.neighbourhoods.size();
        generateNeighbourhoods(1, domain, model.neighbourhoods);
        model.neighbourhoodVarMapping.insert(
            model.neighbourhoodVarMapping.end(),
            model.neighbourhoods.size() - previousNumberNeighbourhoods, i);
        model.varNeighbourhoodMapping.emplace_back(
            model.neighbourhoods.size() - previousNumberNeighbourhoods);
        std::iota(model.varNeighbourhoodMapping.back().begin(),
                  model.varNeighbourhoodMapping.back().end(),
                  previousNumberNeighbourhoods);
        for (auto iter =
                 model.neighbourhoods.begin() + previousNumberNeighbourhoods;
             iter != model.neighbourhoods.end(); ++iter) {
            iter->name = model.variableNames[i] + "_" + iter->name;
        }
    }
    if (model.neighbourhoods.empty()) {
        cerr << "Could not create any neighbourhoods\n";
        throw EndOfSearchException();
    }
}

void ModelBuilder::addConstraints() {
    if (model.optimiseMode != OptimiseMode::NONE) {
        addConstraint(OpMaker<OpIsDefined<IntView>>::make(model.objective));
    }
    // insure that defining expressions, expressions that define a variable are
    // always in the domainof the variable being defined
    for (auto& indexExprPair : model.definingExpressions) {
        size_t varIndex = indexExprPair.first;
        debug_code(assert(varIndex < model.variables.size()));
        auto& expr = indexExprPair.second;
        mpark::visit(
            [&](auto& expr) {
                typedef viewType(expr) View;
                typedef typename AssociatedValueType<View>::type Value;
                typedef typename AssociatedDomain<Value>::type Domain;
                auto& domain = mpark::get<shared_ptr<Domain>>(
                    model.variables[varIndex].first);
                addConstraint(OpMaker<OpInDomain<View>>::make(domain, expr));
            },
            expr);
    }
    model.csp = std::make_shared<OpAnd>(
        std::make_shared<OpSequenceLit>(move(constraints)));
}

Model ModelBuilder::build() {
    std::clock_t startBuildTime = std::clock();
    addConstraints();
    createNeighbourhoods();
    substituteVarsToBeDefined();
    ExprRef<BoolView> cspExpr(model.csp);
    model.csp->optimise(PathExtension::begin(cspExpr));
    for (auto& nameExprPair : model.definingExpressions) {
        mpark::visit(
            [&](auto& expr) { expr->optimise(PathExtension::begin(expr)); },
            nameExprPair.second);
    }
    if (model.optimiseMode != OptimiseMode::NONE) {
        model.objective->optimise(PathExtension::begin(model.objective));
    }
    std::clock_t endBuildTime = std::clock();
    std::cout << "Model build time: "
              << ((double)(endBuildTime - startBuildTime) / CLOCKS_PER_SEC)
              << std::endl;
    return std::move(model);
}

void ModelBuilder::substituteVarsToBeDefined() {
    for (auto& var : varsToBeDefined) {
        auto func = makeFindReplaceFunc(
            var, model.definingExpressions.at(valBase(var).id));
        model.csp->operand = findAndReplace(model.csp->operand, func);
        model.objective = findAndReplace(model.objective, func);
        for (auto& varIdExprPair : model.definingExpressions) {
            if (varIdExprPair.first != valBase(var).id) {
                mpark::visit(
                    [&](auto& expr) { expr = findAndReplace(expr, func); },
                    varIdExprPair.second);
            }
        }
    }
}

FindAndReplaceFunction ModelBuilder::makeFindReplaceFunc(AnyValRef& var,
                                                         AnyExprRef& expr) {
    return mpark::visit(
        [&](auto& var) -> FindAndReplaceFunction {
            typedef typename AssociatedViewType<valType(var)>::type ViewType;
            auto& exprImpl = mpark::get<ExprRef<ViewType>>(expr);
            return [this, var,
                    exprImpl](AnyExprRef ref) -> std::pair<bool, AnyExprRef> {
                auto exprRefTest = mpark::get_if<ExprRef<ViewType>>(&ref);
                if (exprRefTest) {
                    auto valRefTest = this->getIfNonConstValue(*exprRefTest);
                    if (valRefTest && (valBase(valRefTest) == valBase(var))) {
                        return std::make_pair(true, exprImpl);
                    }
                }
                return std::make_pair(false, ref);
            };
        },
        var);
}

size_t randomSize(size_t minSize, size_t maxSize) {
    return globalRandom<size_t>(minSize, std::min(maxSize, minSize + 1000));
}

void Model::printVariables() const {
    for (size_t i = 0; i < variables.size(); ++i) {
        auto& v = variables[i];
        std::cout << "letting " << variableNames[i] << " be ";
        mpark::visit(
            [&](auto& domain) {
                typedef
                    typename BaseType<decltype(domain)>::element_type Domain;
                typedef typename AssociatedValueType<Domain>::type Value;
                typedef typename AssociatedViewType<Value>::type View;
                ExprRef<View> expr(nullptr);
                if (valBase(v.second).container != &definedPool) {
                    expr = mpark::get<ValRef<Value>>(v.second).asExpr();
                } else {
                    expr = mpark::get<ExprRef<View>>(
                        definingExpressions.at(valBase(v.second).id));
                }
                prettyPrint(std::cout, *domain, expr->getViewIfDefined());
            },
            v.first);
        std::cout << std::endl;
    }
}
