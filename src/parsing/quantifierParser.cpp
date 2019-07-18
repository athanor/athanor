#include <algorithm>
#include "operators/opPowerSet.h"
#include "operators/opTupleIndex.h"
#include "operators/quantifier.h"
#include "parsing/parserCommon.h"

#include "types/allVals.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
namespace {

auto fakeTupleDomain(vector<AnyDomainRef> inners) {
    return make_shared<TupleDomain>(move(inners));
}
auto fakeSequenceDomain(AnyDomainRef domain) {
    return make_shared<SequenceDomain>(maxSize(numeric_limits<UInt>().max()),
                                       domain);
}
auto fakeIntDomain = make_shared<IntDomain>(vector<pair<Int, Int>>({{0, 0}}));
}  // namespace
ParseResult parseDomainGenerator(json& domainExpr, ParsedModel& parsedModel);
template <typename Domain, typename View>
using EnableIfDomainMatchesView = typename enable_if<
    is_same<typename AssociatedViewType<
                typename AssociatedValueType<Domain>::type>::type,
            View>::value,
    int>::type;

template <typename Domain, typename View>
using EnableIfDomainNotMatchView = typename enable_if<
    !is_same<typename AssociatedViewType<
                 typename AssociatedValueType<Domain>::type>::type,
             View>::value,
    int>::type;

void checkTuplePatternMatchSize(json& tupleMatchExpr,
                                const shared_ptr<TupleDomain>& domain) {
    if (tupleMatchExpr.size() != domain->inners.size()) {
        cerr << "Error, given pattern match assumes exactly "
             << tupleMatchExpr.size()
             << " members to be present in tuple.  However, "
                "it appears "
                "that the number of members in the "
                "tuple is "
             << domain->inners.size() << ".\n";
        cerr << "expr: " << tupleMatchExpr << "\ntuple domain: " << *domain
             << endl;
        abort();
    }
}

template <typename Domain,
          typename View = typename AssociatedViewType<
              typename AssociatedValueType<Domain>::type>::type>
ExprRef<View> makeTupleIndexFromDomain(shared_ptr<Domain>&,
                                       ExprRef<TupleView>& expr, UInt index) {
    return OpMaker<OpTupleIndex<View>>::make(expr, index);
}

template <typename Domain,
          typename View = typename AssociatedViewType<
              typename AssociatedValueType<Domain>::type>::type>
ExprRef<View> makeSetIndexFromDomain(shared_ptr<Domain>&, ExprRef<SetView>& set,
                                     UInt index) {
    return OpMaker<OpSetIndexInternal<View>>::make(set, index);
}

template <typename DomainType, typename ViewType,
          EnableIfDomainNotMatchView<DomainType, ViewType> = 0>
void addLocalVarsToScopeFromPatternMatch(json&, const shared_ptr<DomainType>&,
                                         ExprRef<ViewType>&, ParsedModel&,
                                         vector<string>&) {
    shouldNotBeCalledPanic;
}

template <typename DomainType, typename ViewType,
          EnableIfDomainMatchesView<DomainType, ViewType> = 0>
void addLocalVarsToScopeFromPatternMatch(
    json& patternExpr, const shared_ptr<DomainType>& domain,
    ExprRef<ViewType>& expr, bool containerHasEmptyType,
    ParsedModel& parsedModel, vector<string>& variablesAddedToScope) {
    if (patternExpr.count("Single")) {
        string name = patternExpr["Single"]["Name"];
        variablesAddedToScope.emplace_back(name);
        parsedModel.namedExprs.emplace(
            variablesAddedToScope.back(),
            ParseResult(domain, expr, containerHasEmptyType));
    } else if (patternExpr.count("AbsPatTuple")) {
        overloaded(
            [&](auto&, auto&) {
                cerr << "Error, Found tuple pattern, but in "
                        "this context "
                        "expected a different expression.\n";
                cerr << "Found domain: " << *domain << endl;
                cerr << "Expr: " << patternExpr << endl;
                abort();
            },
            [&](const shared_ptr<TupleDomain>& domain,
                ExprRef<TupleView>& expr) {
                json& tupleMatchExpr = patternExpr["AbsPatTuple"];
                checkTuplePatternMatchSize(tupleMatchExpr, domain);
                for (size_t i = 0; i < tupleMatchExpr.size(); i++) {
                    mpark::visit(
                        [&](auto& innerDomain) {
                            auto tupleIndexExpr =
                                makeTupleIndexFromDomain(innerDomain, expr, i);
                            addLocalVarsToScopeFromPatternMatch(
                                tupleMatchExpr[i], innerDomain, tupleIndexExpr,
                                containerHasEmptyType, parsedModel,
                                variablesAddedToScope);
                        },
                        domain->inners[i]);
                }
            })(domain, expr);
    } else if (patternExpr.count("AbsPatSet")) {
        overloaded(
            [&](auto&, auto&) {
                cerr << "Error, found set pattern, but did "
                        "not expect a set "
                        "pattern in this context\n";
                cerr << "Found domain: " << *domain << endl;
                cerr << "Expr: " << patternExpr << endl;
                abort();
            },
            [&](const shared_ptr<SetDomain>& domain, ExprRef<SetView>& expr) {
                json& setMatchExpr = patternExpr["AbsPatSet"];
                mpark::visit(
                    [&](auto& innerDomain) {
                        for (size_t i = 0; i < setMatchExpr.size(); i++) {
                            auto setIndexExpr =
                                makeSetIndexFromDomain(innerDomain, expr, i);
                            addLocalVarsToScopeFromPatternMatch(
                                setMatchExpr[i], innerDomain, setIndexExpr,
                                containerHasEmptyType, parsedModel,
                                variablesAddedToScope);
                        }
                    },
                    domain->inner);
            })(domain, expr);
    }
}
template <typename Domain, typename Quantifier>
void handleAsPowerSet(json&, const shared_ptr<Domain>&, Quantifier&, bool,
                      OpPowerSet&, vector<string>&, ParsedModel&) {
    shouldNotBeCalledPanic;
}
template <typename Quantifier>
void handleAsPowerSet(json& generatorExpr,
                      const shared_ptr<SetDomain>& iterDomain,
                      Quantifier& quantifier, bool containerHasEmptyType,
                      OpPowerSet& powerSet,
                      vector<string>& variablesAddedToScope,
                      ParsedModel& parsedModel) {
    auto iterRef = quantifier->template newIterRef<SetView>();

    ExprRef<SetView> iter(iterRef);
    addLocalVarsToScopeFromPatternMatch(generatorExpr[0], iterDomain, iter,
                                        containerHasEmptyType, parsedModel,
                                        variablesAddedToScope);
    powerSet.sizeLimit = generatorExpr[0]["AbsPatSet"].size();
}
template <typename ContainerDomainType, typename Quantifier>
void parseGenerator(json& generatorParent,
                    shared_ptr<ContainerDomainType>& containerDomain,
                    Quantifier& quantifier, bool containerHasEmptyType,
                    vector<string>& variablesAddedToScope,
                    ParsedModel& parsedModel) {
    json& generatorExpr = (generatorParent.count("GenInExpr"))
                              ? generatorParent["GenInExpr"]
                              : generatorParent["GenDomainNoRepr"];
    const AnyDomainRef& innerDomain = overloaded(
        [&](const FunctionDomain& dom) { return dom.to; },
        [&](const auto& dom) { return dom.inner; })(*containerDomain);
    mpark::visit(
        [&](const auto& innerDomain) {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<
                typename AssociatedValueType<InnerDomainType>::type>::type
                InnerViewType;
            auto overload = overloaded(
                [&](shared_ptr<SetDomain>&) {
                    auto powerSetTest =
                        getAs<OpPowerSet>(quantifier->container);
                    if (powerSetTest) {
                        handleAsPowerSet(generatorExpr, innerDomain, quantifier,
                                         containerHasEmptyType, *powerSetTest,
                                         variablesAddedToScope, parsedModel);
                    } else {
                        ExprRef<InnerViewType> iter(
                            quantifier->template newIterRef<InnerViewType>());
                        addLocalVarsToScopeFromPatternMatch(
                            generatorExpr[0], innerDomain, iter,
                            containerHasEmptyType, parsedModel,
                            variablesAddedToScope);
                    }
                },
                [&](shared_ptr<MSetDomain>&) {
                    ExprRef<InnerViewType> iter(
                        quantifier->template newIterRef<InnerViewType>());
                    addLocalVarsToScopeFromPatternMatch(
                        generatorExpr[0], innerDomain, iter,
                        containerHasEmptyType, parsedModel,
                        variablesAddedToScope);
                },
                [&](shared_ptr<SequenceDomain>&) {
                    auto iterRef = quantifier->template newIterRef<TupleView>();
                    if (generatorParent.count("GenDomainNoRepr")) {
                        auto iter = OpMaker<OpTupleIndex<InnerViewType>>::make(
                            iterRef, 1);
                        addLocalVarsToScopeFromPatternMatch(
                            generatorExpr[0], innerDomain, iter,
                            containerHasEmptyType, parsedModel,
                            variablesAddedToScope);
                    } else {
                        auto iterDomain =
                            fakeTupleDomain({fakeIntDomain, innerDomain});
                        ExprRef<TupleView> iter(iterRef);
                        addLocalVarsToScopeFromPatternMatch(
                            generatorExpr[0], iterDomain, iter,
                            containerHasEmptyType, parsedModel,
                            variablesAddedToScope);
                    }
                },
                [&](shared_ptr<FunctionDomain>& functionDomain) {
                    ExprRef<TupleView> iter(
                        quantifier->template newIterRef<TupleView>());
                    auto iterDomain =
                        fakeTupleDomain({functionDomain->from, innerDomain});
                    addLocalVarsToScopeFromPatternMatch(
                        generatorExpr[0], iterDomain, iter,
                        containerHasEmptyType, parsedModel,
                        variablesAddedToScope);
                },
                [&](auto&) {
                    cerr << "no support for this type\n";
                    abort();
                });
            overload(containerDomain);
        },
        innerDomain);
}

template <typename Quantifier>
AnyDomainRef addExprToQuantifier(json& comprExpr, Quantifier& quantifier,
                                 ParsedModel& parsedModel) {
    auto expr = parseExpr(comprExpr[0], parsedModel);
    quantifier->setExpression(expr.expr);
    return expr.domain;
}

template <typename Quantifier>
void addConditionsToQuantifier(json& comprExpr, Quantifier& quantifier,
                               size_t generatorIndex,
                               ParsedModel& parsedModel) {
    vector<ExprRef<BoolView>> conditions;
    for (size_t i = generatorIndex + 1; i < comprExpr[1].size(); i++) {
        auto& expr = comprExpr[1][i];
        if (!expr.count("Condition")) {
            break;
        }
        auto parsedCondition = expect<BoolView>(
            parseExpr(expr["Condition"], parsedModel).expr, [&](auto&&) {
                cerr << "Conitions for quantifiers must be "
                        "bool "
                        "returning.\n";
            });
        conditions.emplace_back(parsedCondition);
    }
    if (conditions.size() > 1) {
        quantifier->setCondition(OpMaker<OpAnd>::make(
            OpMaker<OpSequenceLit>::make(move(conditions))));
    } else if (conditions.size() == 1) {
        quantifier->setCondition(conditions[0]);
    }
}

ParseResult makeFlatten(ParseResult sequenceExpr) {
    auto& domain = mpark::get<shared_ptr<SequenceDomain>>(sequenceExpr.domain);
    auto& expr = mpark::get<ExprRef<SequenceView>>(sequenceExpr.expr);
    return mpark::visit(
        [&](auto& innerDomain) {
            typedef typename AssociatedValueType<typename BaseType<decltype(
                innerDomain)>::element_type>::type ValueType;
            typedef typename AssociatedViewType<ValueType>::type View;
            return ParseResult(sequenceExpr.domain,
                               OpMaker<OpFlattenOneLevel<View>>::make(expr),
                               sequenceExpr.hasEmptyType);
        },
        domain->inner);
}

optional<ParseResult> parseComprehensionImpl(json& comprExpr,
                                             ParsedModel& parsedModel,
                                             size_t generatorIndex);
template <typename ContainerReturnType, typename ContainerDomainType>
ParseResult buildQuant(json& comprExpr, ExprRef<ContainerReturnType>& container,
                       shared_ptr<ContainerDomainType>& domain,
                       bool containerHasEmptyType, size_t generatorIndex,
                       ParsedModel& parsedModel) {
    auto quantifier = make_shared<Quantifier<ContainerReturnType>>(container);
    vector<string> variablesAddedToScope;
    parseGenerator(comprExpr[1][generatorIndex]["Generator"], domain,
                   quantifier, containerHasEmptyType, variablesAddedToScope,
                   parsedModel);
    addConditionsToQuantifier(comprExpr, quantifier, generatorIndex,
                              parsedModel);

    optional<ParseResult> returnValue;
    auto innerQuantifier =
        parseComprehensionImpl(comprExpr, parsedModel, generatorIndex + 1);
    if (innerQuantifier) {
        quantifier->setExpression(innerQuantifier->expr);
        returnValue = makeFlatten(ParseResult(
            innerQuantifier->domain, ExprRef<SequenceView>(quantifier),
            containerHasEmptyType || innerQuantifier->hasEmptyType));
    } else {
        AnyDomainRef innerDomain =
            addExprToQuantifier(comprExpr, quantifier, parsedModel);
        returnValue = ParseResult(fakeSequenceDomain(innerDomain),
                                  ExprRef<SequenceView>(quantifier),
                                  containerHasEmptyType);
    }
    for (auto& varName : variablesAddedToScope) {
        parsedModel.namedExprs.erase(varName);
    }
    return *returnValue;
}
// this function is mutually recursive with buildQuant
optional<ParseResult> parseComprehensionImpl(json& comprExpr,
                                             ParsedModel& parsedModel,
                                             size_t generatorIndex) {
    // if there are no more generators to parse, return ull
    if (generatorIndex == comprExpr[1].size()) {
        return nullopt;
    }
    // if generator index does not point to a gnerator, skip to the next one
    if (!comprExpr[1][generatorIndex].count("Generator")) {
        return parseComprehensionImpl(comprExpr, parsedModel,
                                      generatorIndex + 1);
    }

    auto& generatorParent = comprExpr[1][generatorIndex]["Generator"];
    // check if we are generating from a domain or iterating from a container
    // like a set or sequence.
    json& generatorExpr = (generatorParent.count("GenInExpr"))
                              ? generatorParent["GenInExpr"]
                              : generatorParent["GenDomainNoRepr"];

    auto quantifyingOver =
        (generatorParent.count("GenInExpr"))
            ? parseExpr(generatorExpr[1], parsedModel)
            : parseDomainGenerator(generatorExpr[1], parsedModel);
    auto overload = overloaded(
        [&](ExprRef<SetView>& set) {
            auto& domain =
                mpark::get<shared_ptr<SetDomain>>(quantifyingOver.domain);
            return buildQuant(comprExpr, set, domain,
                              quantifyingOver.hasEmptyType, generatorIndex,
                              parsedModel);
        },
        [&](ExprRef<MSetView>& mSet) {
            auto& domain =
                mpark::get<shared_ptr<MSetDomain>>(quantifyingOver.domain);
            return buildQuant(comprExpr, mSet, domain,
                              quantifyingOver.hasEmptyType, generatorIndex,
                              parsedModel);
        },
        [&](ExprRef<SequenceView>& sequence) {
            auto& domain =
                mpark::get<shared_ptr<SequenceDomain>>(quantifyingOver.domain);
            return buildQuant(comprExpr, sequence, domain,
                              quantifyingOver.hasEmptyType, generatorIndex,
                              parsedModel);
        },
        [&](ExprRef<FunctionView>& function) {
            auto& domain =
                mpark::get<shared_ptr<FunctionDomain>>(quantifyingOver.domain);
            return buildQuant(comprExpr, function, domain,
                              quantifyingOver.hasEmptyType, generatorIndex,
                              parsedModel);
        },
        [&](auto &&) -> ParseResult {
            cerr << "Error, not yet handling quantifier for "
                    "this type: "
                 << generatorExpr << endl;
            abort();
        });

    return mpark::visit(overload, quantifyingOver.expr);
}

ParseResult parseComprehension(json& comprExpr, ParsedModel& parsedModel) {
    auto compr = parseComprehensionImpl(comprExpr, parsedModel, 0);
    if (compr) {
        return move(*compr);
    } else {
        cerr << "Failed to find a generator to parse in the "
                "comprehension.\n";
        abort();
    }
}

ParseResult makeDomainGeneratorFromIntDomain(
    const shared_ptr<IntDomain>& domain) {
    if (domain->bounds.size() != 1) {
        cerr << "Do not currently support unrolling over "
                "int domains with "
                "holes.\n";
        abort();
    }
    auto from = make<IntValue>();
    from->value = domain->bounds.front().first;

    auto to = make<IntValue>();
    to->value = domain->bounds.front().second;

    return ParseResult(fakeSequenceDomain(domain),
                       OpMaker<IntRange>::make(from.asExpr(), to.asExpr()),
                       false);
}

ParseResult makeDomainGeneratorFromEnumDomain(
    const shared_ptr<EnumDomain>& domain) {
    return ParseResult(fakeSequenceDomain(domain),
                       OpMaker<EnumRange>::make(domain), false);
}

ParseResult parseDomainGenerator(json& domainExpr, ParsedModel& parsedModel) {
    auto domain = parseDomain(domainExpr, parsedModel);
    return mpark::visit(
        overloaded(
            [&](shared_ptr<IntDomain>& domain) {
                return makeDomainGeneratorFromIntDomain(domain);
            },
            [&](shared_ptr<EnumDomain>& domain) {
                return makeDomainGeneratorFromEnumDomain(domain);
            },
            [&](auto& domain) -> ParseResult {
                cerr << "Error: do not yet support "
                        "unrolling this domain.\n";
                prettyPrint(cerr, domain) << endl;
                cerr << domainExpr << endl;
                abort();
            }),
        domain);
}

ParseResult quantifyOverSet(shared_ptr<SetDomain>& domain,
                            ExprRef<SetView>& expr, bool hasEmptyType) {
    auto quant = make_shared<Quantifier<SetView>>(expr);
    return mpark::visit(
        [&](auto& innerDomain) {
            typedef typename AssociatedValueType<typename BaseType<decltype(
                innerDomain)>::element_type>::type InnerValueType;
            typedef
                typename AssociatedViewType<InnerValueType>::type InnerViewType;
            quant->setExpression(
                ExprRef<InnerViewType>(quant->newIterRef<InnerViewType>()));
            return ParseResult(fakeSequenceDomain(innerDomain),
                               ExprRef<SequenceView>(quant), hasEmptyType);
        },
        domain->inner);
}

ParseResult toSequence(ParseResult parsedExpr) {
    auto overload = overloaded(
        [&](shared_ptr<SequenceDomain>&) { return parsedExpr; },
        [&](shared_ptr<SetDomain>& domain) {
            return quantifyOverSet(
                domain, mpark::get<ExprRef<SetView>>(parsedExpr.expr),
                parsedExpr.hasEmptyType);
        },
        [&](auto&) -> ParseResult { todoImpl(); });
    return mpark::visit(overload, parsedExpr.domain);
}