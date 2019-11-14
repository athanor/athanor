#include "search/model.h"
#include <iostream>
#include "search/endOfSearchException.h"
#include "search/statsContainer.h"
#ifdef WASM_TARGET
#include <emscripten/bind.h>
#endif

using namespace std;
void ModelBuilder::createNeighbourhoods() {
    for (size_t i = 0; i < model.variables.size(); ++i) {
        if (valBase(model.variables[i].second).container == &inlinedPool) {
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
        iota(model.varNeighbourhoodMapping.back().begin(),
             model.varNeighbourhoodMapping.back().end(),
             previousNumberNeighbourhoods);
        for (auto iter =
                 model.neighbourhoods.begin() + previousNumberNeighbourhoods;
             iter != model.neighbourhoods.end(); ++iter) {
            iter->name = model.variableNames[i] + "_" + iter->name;
        }
    }
    if (model.neighbourhoods.empty()) {
        cout << "Could not create any neighbourhoods\n";
    }
}

void addConstraintsOnVarsToBeSubstituted(ModelBuilder& builder, Model& model) {
    // insure that defining expressions, expressions that define a variable are
    // always in the domainof the variable being defined
    for (auto& indexExprPair : model.definingExpressions) {
        size_t varIndex = indexExprPair.first;
        debug_code(assert(varIndex < model.variables.size()));
        lib::visit(
            [&](auto& var) {
                typedef valType(var) Value;
                typedef typename AssociatedViewType<Value>::type View;
                typedef typename AssociatedDomain<Value>::type Domain;
                if (is_same<Domain, BoolDomain>::value) {
                    return;
                }
                auto& domain = lib::get<shared_ptr<Domain>>(
                    model.variables[varIndex].first);
                /*we don't actually put the OpInDomain constraint on the expr
                 * directly.  Instead, we put it on the variable.  Later after
                 * optimisations have been applied, all instances of the
                 * variable will be replaced withthe the defining expression. */
                builder.addConstraint(
                    OpMaker<OpInDomain<View>>::make(domain, var.asExpr()));
            },
            model.variables[varIndex].second);
    }
}

template <typename View>
void optimiseExpr(ExprRef<View>& expr) {
    while (::optimise(expr)) {
        // repeat until no more optimisations
    }
}

static void optimiseModel(ModelBuilder& builder, Model& model) {
    for (auto& constraint : builder.constraints) {
        optimiseExpr(constraint);
    }
    for (auto& nameExprPair : model.definingExpressions) {
        lib::visit([&](auto& expr) { optimiseExpr(expr); },
                   nameExprPair.second);
    }
    lib::visit([&](auto& objective) { optimiseExpr(objective); },
               model.objective);
}

Model ModelBuilder::build() {
    clock_t startBuildTime = clock();
    addConstraintsOnVarsToBeSubstituted(*this, model);
    optimiseModel(*this, model);
    // calling optimise here as want to optimise model before performing the
    // following actions like substituting vars and posting objective
    // constraints.

    substituteVarsToBeDefined();
    if (model.optimiseMode != OptimiseMode::NONE) {
        lib::visit(
            [&](auto& objective) {
                addConstraint(
                    OpMaker<OpIsDefined<viewType(objective)>>::make(objective));
            },
            model.objective);
    }
    model.csp =
        make_shared<OpAnd>(make_shared<OpSequenceLit>(move(constraints)));
    optimiseExpr(model.csp);
    createNeighbourhoods();

    clock_t endBuildTime = clock();
    cout << "Model build time (CPU): "
         << ((double)(endBuildTime - startBuildTime) / CLOCKS_PER_SEC) << "s\n";
    return move(model);
}

void ModelBuilder::substituteVarsToBeDefined() {
    for (auto& var : varsToBeDefined) {
        auto func = makeFindReplaceFunc(
            var, model.definingExpressions.at(valBase(var).id));
        for (auto& constraint : constraints) {
            constraint = findAndReplace(constraint, func);
        }
        lib::visit(
            [&](auto& objective) {
                objective = findAndReplace(objective, func);
            },
            model.objective);
        for (auto& varIdExprPair : model.definingExpressions) {
            if (varIdExprPair.first != valBase(var).id) {
                lib::visit(
                    [&](auto& expr) { expr = findAndReplace(expr, func); },
                    varIdExprPair.second);
            }
        }
    }
}

FindAndReplaceFunction ModelBuilder::makeFindReplaceFunc(AnyValRef& var,
                                                         AnyExprRef& expr) {
    return lib::visit(
        [&](auto& var) -> FindAndReplaceFunction {
            typedef typename AssociatedViewType<valType(var)>::type ViewType;
            auto& exprImpl = lib::get<ExprRef<ViewType>>(expr);
            return [this, var, exprImpl](
                       AnyExprRef ref,
                       const PathExtension&) -> pair<bool, AnyExprRef> {
                auto exprRefTest = lib::get_if<ExprRef<ViewType>>(&ref);
                if (exprRefTest) {
                    auto valRefTest = this->getIfNonConstValue(*exprRefTest);
                    if (valRefTest && (valBase(valRefTest) == valBase(var))) {
                        return make_pair(true, exprImpl);
                    }
                }
                return make_pair(false, ref);
            };
        },
        var);
}

void printUnnamedType(ostream& os, const EnumDomain& domain) {
    os << "{";
    for (size_t i = 0; i < domain.numberValues(); i++) {
        if (i != 0) {
            os << ",";
        }
        domain.printValue(os, i);
    }
    os << "}";
}

extern string bestSolution;
extern bool saveBestSolution;
ostringstream bestSolutionStream;
ostream& getBestSolutionStream() {
    if (saveBestSolution) {
        bestSolutionStream.str("");
        bestSolutionStream.clear();
        return bestSolutionStream;
    } else {
        return cout;
    }
}

void dumpSolution() {
    if (saveBestSolution) {
        bestSolution = bestSolutionStream.str();
        cout << bestSolution;
    }
}

void Model::printVariables() const {
#ifdef WASM_TARGET
    using namespace emscripten;
    ostringstream os;
#define varName(x) "<var>" << x << "</var>"
    os << "<code>";
#else
    auto& os = getBestSolutionStream();
#define varName(x) x
#endif
    for (auto& domain : unnamedTypes) {
        os << "letting " << varName(domain->domainName) << " be new type enum ";
        printUnnamedType(os, *domain);
        os << endl;
    }
    for (size_t i = 0; i < variables.size(); ++i) {
        auto& v = variables[i];
        os << "letting " << varName(variableNames[i]) << " be ";
        lib::visit(
            [&](auto& domain) {
                typedef
                    typename BaseType<decltype(domain)>::element_type Domain;
                typedef typename AssociatedValueType<Domain>::type Value;
                typedef typename AssociatedViewType<Value>::type View;
                ExprRef<View> expr(nullptr);
                if (valBase(v.second).container != &inlinedPool) {
                    expr = lib::get<ValRef<Value>>(v.second).asExpr();
                } else {
                    expr = lib::get<ExprRef<View>>(
                        definingExpressions.at(valBase(v.second).id));
                }
                prettyPrint(os, *domain, expr->getViewIfDefined());
            },
            v.first);
        os << endl;
    }
#ifdef WASM_TARGET
    os << "</code>";
    val::global().call<void>("printSolution", os.str());
#endif
    dumpSolution();
}

void Model::debugSanityCheck() const {
    csp->debugSanityCheck();
    lib::visit([&](auto& objective) { objective->debugSanityCheck(); },
               objective);
    for (auto& indexExprPair : definingExpressions) {
        lib::visit([&](auto& expr) { expr->debugSanityCheck(); },
                   indexExprPair.second);
    }
    sanityCheckRepeatMode = !sanityCheckRepeatMode;
}

Objective Model::getObjective() const {
    return lib::visit(overloaded(
                          [&](const ExprRef<IntView>& e) {
                              return Objective(optimiseMode, e);
                          },
                          [&](const ExprRef<TupleView>& e) {
                              return Objective(optimiseMode, e);
                          },
                          [&](const auto& e) -> Objective {
                              myCerr << "unsupported objective type\n";
                              myCerr << "value = " << e << endl;
                              todoImpl();
                          }),
                      objective);
}

bool Model::objectiveDefined() const {
    return lib::visit(
        [&](const auto& objective) {
            return objective->getViewIfDefined().hasValue();
        },
        objective);
}
