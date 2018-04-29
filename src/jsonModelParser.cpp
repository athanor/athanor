#include "jsonModelParser.h"
#include <functional>
#include <iostream>
#include <json.hpp>
#include <unordered_map>
#include <utility>
#include "common/common.h"
#include "operators/operatorMakers.h"
#include "operators/quantifier.h"
#include "search/model.h"
#include "types/allTypes.h"

using namespace std;
using namespace nlohmann;
auto fakeIntDomain =
    make_shared<IntDomain>(vector<pair<Int, Int>>({intBound(0, 0)}));
auto fakeBoolDomain = make_shared<BoolDomain>();
shared_ptr<SetDomain> fakeSetDomain(const AnyDomainRef& ref) {
    return make_shared<SetDomain>(exactSize(0), ref);
}

shared_ptr<MSetDomain> fakeMSetDomain(const AnyDomainRef& ref) {
    return make_shared<MSetDomain>(exactSize(0), ref);
}

shared_ptr<SequenceDomain> fakeSequenceDomain(const AnyDomainRef& ref) {
    return make_shared<SequenceDomain>(exactSize(0), ref);
}

shared_ptr<TupleDomain> fakeTupleDomain(std::vector<AnyDomainRef> domains) {
    return make_shared<TupleDomain>(move(domains));
}

ParsedModel::ParsedModel() : builder(make_unique<ModelBuilder>()) {}
typedef function<pair<AnyDomainRef, AnyExprRef>(json&, ParsedModel&)>
    ParseExprFunction;
typedef function<AnyDomainRef(json&, ParsedModel&)> ParseDomainFunction;

template <typename Function, typename DefaultValue,
          typename ReturnType = typename Function::result_type>
pair<bool, ReturnType> stringMatch(const vector<pair<string, Function>>& match,
                                   const DefaultValue& defaultValue,
                                   json& essenceExpr,
                                   ParsedModel& parsedModel) {
    for (auto& matchCase : match) {
        if (essenceExpr.count(matchCase.first)) {
            return make_pair(
                true,
                matchCase.second(essenceExpr[matchCase.first], parsedModel));
        }
    }
    return make_pair(false, defaultValue);
}

pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseValue(
    json& essenceExpr, ParsedModel& parsedModel);
pair<bool, AnyDomainRef> tryParseDomain(json& essenceExpr,
                                        ParsedModel& parsedModel);
pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseExpr(
    json& essenceExpr, ParsedModel& parsedModel);

pair<AnyDomainRef, AnyExprRef> parseExpr(json& essenceExpr,
                                         ParsedModel& parsedModel) {
    auto boolConstraintPair = tryParseExpr(essenceExpr, parsedModel);
    if (boolConstraintPair.first) {
        return move(boolConstraintPair.second);
    } else {
        cerr << "Failed to parse expression: " << essenceExpr << endl;
        abort();
    }
}

pair<AnyDomainRef, AnyExprRef> parseValue(json& essenceExpr,
                                          ParsedModel& parsedModel) {
    auto boolValuePair = tryParseValue(essenceExpr, parsedModel);
    if (boolValuePair.first) {
        return move(boolValuePair.second);
    } else {
        cerr << "Failed to parse value: " << essenceExpr << endl;
        abort();
    }
}

AnyValRef toValRef(const AnyExprRef& op) {
    return mpark::visit(
        [&](auto& ref) -> AnyValRef {
            typedef typename AssociatedValueType<viewType(ref)>::type ValType;
            auto val = make<ValType>();
            val->initFrom(ref->view());
            return val;
        },
        op);
}

Int parseValueAsInt(json& essenceExpr, ParsedModel& parsedModel) {
    return mpark::get<ValRef<IntValue>>(
               toValRef(parseExpr(essenceExpr, parsedModel).second))
        ->value;
}

AnyDomainRef parseDomain(json& essenceExpr, ParsedModel& parsedModel) {
    auto boolDomainPair = tryParseDomain(essenceExpr, parsedModel);
    if (boolDomainPair.first) {
        return move(boolDomainPair.second);
    } else {
        cerr << "Failed to parse domain: " << essenceExpr << endl;
        abort();
    }
}

pair<shared_ptr<SetDomain>, ExprRef<SetView>> parseConstantSet(
    json& essenceSetConstant, ParsedModel& parsedModel) {
    ValRef<SetValue> val = make<SetValue>();
    if (essenceSetConstant.size() == 0) {
        val->setInnerType<IntView>();
        cerr << "Not sure how to work out type of empty set yet, will handle "
                "this later.";
        todoImpl();
    }
    return mpark::visit(
        [&](auto&& innerDomain) {
            typedef typename AssociatedValueType<typename BaseType<decltype(
                innerDomain)>::element_type>::type InnerValueType;
            val->setInnerType<
                typename AssociatedViewType<InnerValueType>::type>();
            for (size_t i = 0; i < essenceSetConstant.size(); ++i) {
                val->addMember(mpark::get<ValRef<InnerValueType>>(toValRef(
                    parseExpr(essenceSetConstant[i], parsedModel).second)));
            }
            return make_pair(make_shared<SetDomain>(
                                 exactSize(val->numberElements()), innerDomain),
                             val.asExpr());
        },
        parseValue(essenceSetConstant[0], parsedModel).first);
}

pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseValue(
    json& essenceExpr, ParsedModel& parsedModel) {
    if (essenceExpr.count("Constant")) {
        return tryParseValue(essenceExpr["Constant"], parsedModel);
    } else if (essenceExpr.count("ConstantAbstract")) {
        return tryParseValue(essenceExpr["ConstantAbstract"], parsedModel);
    } else if (essenceExpr.count("AbstractLiteral")) {
        return tryParseValue(essenceExpr["AbstractLiteral"], parsedModel);
    } else if (essenceExpr.count("ConstantInt")) {
        auto val = make<IntValue>();
        val->value = essenceExpr["ConstantInt"];
        return make_pair(true, make_pair(fakeIntDomain, val.asExpr()));
    } else if (essenceExpr.count("ConstantBool")) {
        auto val = make<BoolValue>();
        val->violation = (bool(essenceExpr["ConstantBool"])) ? 0 : 1;
        return make_pair(true, make_pair(fakeBoolDomain, val.asExpr()));
    } else if (essenceExpr.count("AbsLitSet")) {
        return make_pair(
            true, parseConstantSet(essenceExpr["AbsLitSet"], parsedModel));
    } else if (essenceExpr.count("Reference")) {
        auto& essenceReference = essenceExpr["Reference"];
        string referenceName = essenceReference[0]["Name"];
        if (parsedModel.namedExprs.count(referenceName)) {
            return make_pair(true, parsedModel.namedExprs.at(referenceName));
        } else {
            cerr << "Found reference to value with name \"" << referenceName
                 << "\" but this does not appear to be in scope.\n"
                 << essenceExpr << endl;
            abort();
        }
    }
    return make_pair(false,
                     make_pair(AnyDomainRef(shared_ptr<IntDomain>(nullptr)),
                               AnyExprRef(ExprRef<IntView>(nullptr))));
}

shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel) {
    vector<pair<Int, Int>> ranges;
    for (auto& rangeExpr : intDomainExpr) {
        Int from, to;

        if (rangeExpr.count("RangeBounded")) {
            from = parseValueAsInt(rangeExpr["RangeBounded"][0], parsedModel);
            to = parseValueAsInt(rangeExpr["RangeBounded"][1], parsedModel);
        } else if (rangeExpr.count("RangeSingle")) {
            from = parseValueAsInt(rangeExpr["RangeSingle"], parsedModel);
            to = from;
        } else {
            cerr << "Unrecognised type of int range: " << rangeExpr << endl;
            abort();
        }
        ranges.emplace_back(from, to);
    }
    return make_shared<IntDomain>(move(ranges));
}

shared_ptr<BoolDomain> parseDomainBool(json&, ParsedModel&) {
    return fakeBoolDomain;
}

SizeAttr parseSizeAttr(json& sizeAttrExpr, ParsedModel& parsedModel) {
    if (sizeAttrExpr.count("SizeAttr_None")) {
        return noSize();
    } else if (sizeAttrExpr.count("SizeAttr_MinSize")) {
        return minSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_MinSize"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_MaxSize")) {
        return maxSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_MaxSize"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_Size")) {
        return exactSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_Size"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_MinMaxSize")) {
        auto& sizeRangeExpr = sizeAttrExpr["SizeAttr_MinMaxSize"];
        return sizeRange(parseValueAsInt(sizeRangeExpr[0], parsedModel),
                         parseValueAsInt(sizeRangeExpr[1], parsedModel));
    } else {
        cerr << "Could not parse this as a size attribute: " << sizeAttrExpr
             << endl;
        abort();
    }
}

shared_ptr<SetDomain> parseDomainSet(json& setDomainExpr,
                                     ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(setDomainExpr[1], parsedModel);
    return make_shared<SetDomain>(sizeAttr,
                                  parseDomain(setDomainExpr[2], parsedModel));
}

shared_ptr<MSetDomain> parseDomainMSet(json& mSetDomainExpr,
                                       ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(mSetDomainExpr[1][0], parsedModel);
    if (!mSetDomainExpr[1][1].count("OccurAttr_None")) {
        cerr << "Error: for the moment, given attribute must be "
                "OccurAttr_None.  This is not handled yet: "
             << mSetDomainExpr[1][1] << endl;
        abort();
    }
    return make_shared<MSetDomain>(sizeAttr,
                                   parseDomain(mSetDomainExpr[2], parsedModel));
}

shared_ptr<SequenceDomain> parseDomainSequence(json& sequenceDomainExpr,
                                               ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(sequenceDomainExpr[1][0], parsedModel);
    if (sequenceDomainExpr[1][1] != "JectivityAttr_None") {
        cerr << "Error: for the moment, given attribute must be "
                "JectivityAttr_None.  This is not handled yet: "
             << sequenceDomainExpr[1][1] << endl;
        abort();
    }
    return make_shared<SequenceDomain>(
        sizeAttr, parseDomain(sequenceDomainExpr[2], parsedModel));
}

shared_ptr<TupleDomain> parseDomainTuple(json& tupleDomainExpr,
                                         ParsedModel& parsedModel) {
    vector<AnyDomainRef> innerDomains;
    for (auto& innerDomainExpr : tupleDomainExpr) {
        innerDomains.emplace_back(parseDomain(innerDomainExpr, parsedModel));
    }
    return make_shared<TupleDomain>(move(innerDomains));
}

AnyDomainRef parseDomainReference(json& domainReference,
                                  ParsedModel& parsedModel) {
    string referenceName = domainReference[0]["Name"];
    if (!parsedModel.domainLettings.count(referenceName)) {
        cerr << "Found reference to domainwith name \"" << referenceName
             << "\" but this does not appear to be in scope.\n"
             << domainReference << endl;
        abort();
    } else {
        return parsedModel.domainLettings.at(referenceName);
    }
}

pair<bool, AnyDomainRef> tryParseDomain(json& domainExpr,
                                        ParsedModel& parsedModel) {
    return stringMatch<ParseDomainFunction>(
        {{"DomainInt", parseDomainInt},
         {"DomainBool", parseDomainBool},
         {"DomainSet", parseDomainSet},
         {"DomainMSet", parseDomainMSet},
         {"DomainSequence", parseDomainSequence},
         {"DomainTuple", parseDomainTuple},
         {"DomainReference", parseDomainReference}},
        AnyDomainRef(fakeIntDomain), domainExpr, parsedModel);
}

template <typename RetType, typename Constraint, typename Func>
ExprRef<RetType> expect(Constraint&& constraint, Func&& func) {
    return mpark::visit(
        overloaded(
            [&](ExprRef<RetType>& constraint) -> ExprRef<RetType> {
                return move(constraint);
            },
            [&](auto&& constraint) -> ExprRef<RetType> {
                func(forward<decltype(constraint)>(constraint));
                cerr << "\nType found was instead "
                     << TypeAsString<typename AssociatedValueType<viewType(
                            constraint)>::type>::value
                     << endl;
                abort();
            }),
        constraint);
}

pair<AnyDomainRef, AnyExprRef> parseOpMod(json& modExpr,
                                          ParsedModel& parsedModel) {
    string errorMessage = "Expected int returning expression within Op mod: ";
    ExprRef<IntView> left =
        expect<IntView>(parseExpr(modExpr[0], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << modExpr[0]; });
    ExprRef<IntView> right =
        expect<IntView>(parseExpr(modExpr[1], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << modExpr[1]; });
    return make_pair(fakeIntDomain,
                     OpMaker<OpMod>::make(move(left), move(right)));
}

pair<AnyDomainRef, AnyExprRef> parseOpSubsetEq(json& subsetExpr,
                                               ParsedModel& parsedModel) {
    string errorMessage =
        "Expected set returning expression within Op subset: ";
    ExprRef<SetView> left =
        expect<SetView>(parseExpr(subsetExpr[0], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << subsetExpr[0]; });
    ExprRef<SetView> right =
        expect<SetView>(parseExpr(subsetExpr[1], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << subsetExpr[1]; });
    return make_pair(fakeBoolDomain,
                     OpMaker<OpSubsetEq>::make(move(left), move(right)));
}

pair<AnyDomainRef, AnyExprRef> parseOpTwoBars(json& operandExpr,
                                              ParsedModel& parsedModel) {
    AnyExprRef operand = parseExpr(operandExpr, parsedModel).second;
    return mpark::visit(
        overloaded(
            [&](ExprRef<SetView>& set)
                -> pair<shared_ptr<IntDomain>, AnyExprRef> {
                return make_pair(fakeIntDomain, OpMaker<OpSetSize>::make(set));
            },
            [&](auto&& operand) -> pair<shared_ptr<IntDomain>, AnyExprRef> {
                cerr << "Error, not yet handling OpTwoBars with an operand "
                        "of type "
                     << TypeAsString<typename AssociatedValueType<viewType(
                            operand)>::type>::value
                     << ": " << operandExpr << endl;
                abort();
            }),
        operand);
}
pair<AnyDomainRef, AnyExprRef> parseOpSequenceIndex(
    AnyDomainRef& innerDomain, ExprRef<SequenceView>& sequence, json& indexExpr,
    ParsedModel& parsedModel) {
    auto index = expect<IntView>(
        parseExpr(indexExpr[0], parsedModel).second, [](auto&&) {
            cerr << "Sequence must be indexed by an int expression.\n";
        });
    return mpark::visit(
        [&](auto& innerDomain) -> pair<AnyDomainRef, AnyExprRef> {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<
                typename AssociatedValueType<InnerDomainType>::type>::type View;
            return make_pair(innerDomain,
                             ExprRef<View>(OpMaker<OpSequenceIndex<View>>::make(
                                 sequence, index)));
        },
        innerDomain);
}

pair<AnyDomainRef, AnyExprRef> parseOpTupleIndex(
    shared_ptr<TupleDomain>& tupleDomain, ExprRef<TupleView>& tuple,
    json& indexExpr, ParsedModel& parsedModel) {
    UInt index = parseValueAsInt(indexExpr[0], parsedModel) - 1;
    return mpark::visit(
        [&](auto& innerDomain) -> pair<AnyDomainRef, AnyExprRef> {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<
                typename AssociatedValueType<InnerDomainType>::type>::type View;
            return make_pair(
                innerDomain,
                ExprRef<View>(OpMaker<OpTupleIndex<View>>::make(tuple, index)));
        },
        tupleDomain->inners[index]);
}

pair<AnyDomainRef, AnyExprRef> parseOpRelationProj(json& operandsExpr,
                                                   ParsedModel& parsedModel) {
    auto leftOperand = parseExpr(operandsExpr[0], parsedModel);
    return mpark::visit(
        overloaded(
            [&](ExprRef<SequenceView>& sequence)
                -> pair<AnyDomainRef, AnyExprRef> {
                auto& innerDomain =
                    mpark::get<shared_ptr<SequenceDomain>>(leftOperand.first)
                        ->inner;
                return parseOpSequenceIndex(innerDomain, sequence,
                                            operandsExpr[1], parsedModel);
            },
            [&](ExprRef<TupleView>& tuple) -> pair<AnyDomainRef, AnyExprRef> {
                auto& tupleDomain =
                    mpark::get<shared_ptr<TupleDomain>>(leftOperand.first);
                return parseOpTupleIndex(tupleDomain, tuple, operandsExpr[1],
                                         parsedModel);
            },
            [&](auto&& operand) -> pair<AnyDomainRef, AnyExprRef> {
                cerr << "Error, not yet handling op relation projection with a "
                        "left operand "
                        "of type "
                     << TypeAsString<typename AssociatedValueType<viewType(
                            operand)>::type>::value
                     << ": " << operandsExpr << endl;
                abort();
            }),
        leftOperand.second);
}

pair<AnyDomainRef, AnyExprRef> parseOpEq(json& operandsExpr,
                                         ParsedModel& parsedModel) {
    AnyExprRef left = parseExpr(operandsExpr[0], parsedModel).second;
    AnyExprRef right = parseExpr(operandsExpr[1], parsedModel).second;
    return mpark::visit(
        [&](auto& left) {
            auto errorHandler = [&](auto&&) {
                cerr << "Expected right operand to be of same type as left, "
                        "i.e. "
                     << TypeAsString<typename AssociatedValueType<viewType(
                            left)>::type>::value
                     << endl;
            };
            return overloaded(
                [&](ExprRef<IntView>& left)
                    -> pair<shared_ptr<BoolDomain>, ExprRef<BoolView>> {
                    return make_pair(
                        fakeBoolDomain,
                        OpMaker<OpIntEq>::make(
                            left, expect<IntView>(right, errorHandler)));
                },
                [&](auto&& left)
                    -> pair<shared_ptr<BoolDomain>, ExprRef<BoolView>> {
                    cerr
                        << "Error, not yet handling OpEq with operands of type "
                        << TypeAsString<typename AssociatedValueType<viewType(
                               left)>::type>::value
                        << ": " << operandsExpr << endl;
                    abort();
                })(left);
        },
        left);
}

pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> parseConstantMatrix(
    json& matrixExpr, ParsedModel& parsedModel) {
    if (matrixExpr[1].size() == 0) {
        cerr << "Not sure how to work out type of empty matrixyet, will handle "
                "this later.";
        todoImpl();
    }
    AnyExprVec newSequenceMembers;
    AnyDomainRef innerDomain = fakeIntDomain;
    bool first = true;
    for (auto& elementExpr : matrixExpr[1]) {
        auto expr = parseExpr(elementExpr, parsedModel);
        if (first) {
            innerDomain = expr.first;
        }
        mpark::visit(
            [&](auto& member) {
                if (first) {
                    newSequenceMembers.emplace<ExprRefVec<viewType(member)>>();
                }
                mpark::get<ExprRefVec<viewType(member)>>(newSequenceMembers)
                    .emplace_back(move(member));
            },
            expr.second);
        first = false;
    }
    return make_pair(fakeSequenceDomain(innerDomain),
                     OpMaker<OpSequenceLit>::make(move(newSequenceMembers)));
}
template <typename Domain, typename View>
using EnableIfDomainMatchesView = typename enable_if<
    is_same<typename AssociatedViewType<
                typename AssociatedValueType<Domain>::type>::type,
            View>::value,
    int>::type;
template <typename DomainType, typename ViewType,
          EnableIfDomainMatchesView<DomainType, ViewType> = 0>
void extractPatternMatchAndAddExprsToScope(
    json& patternExpr, const shared_ptr<DomainType>& domain,
    ExprRef<ViewType>& expr, ParsedModel& parsedModel,
    vector<string>& variablesAddedToScope) {
    string name = patternExpr["Single"]["Name"];
    variablesAddedToScope.emplace_back(name);
    parsedModel.namedExprs.emplace(variablesAddedToScope.back(),
                                   make_pair(domain, expr));
}

template <typename ContainerDomainType, typename Quantifier>
AnyDomainRef addExprToQuantifier(
    json& comprExpr, shared_ptr<ContainerDomainType>& containerDomain,
    Quantifier& quantifier, ParsedModel& parsedModel) {
    auto& generatorExpr = comprExpr[1][0]["Generator"]["GenInExpr"];
    vector<string> variablesAddedToScope;
    mpark::visit(
        [&](auto& innerDomain) {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<
                typename AssociatedValueType<InnerDomainType>::type>::type
                InnerViewType;

            if (is_same<TupleDomain, typename BaseType<decltype(
                                         innerDomain)>::element_type>::value) {
                auto iterDomain = fakeTupleDomain({fakeIntDomain, innerDomain});
                auto iter = ExprRef<TupleView>(
                    quantifier->template newIterRef<TupleView>());
                extractPatternMatchAndAddExprsToScope(
                    generatorExpr[0], iterDomain, iter, parsedModel,
                    variablesAddedToScope);
            } else {
                auto iter = ExprRef<InnerViewType>(
                    quantifier->template newIterRef<InnerViewType>());
                extractPatternMatchAndAddExprsToScope(
                    generatorExpr[0], innerDomain, iter, parsedModel,
                    variablesAddedToScope);
            }
        },
        (containerDomain->inner));
    auto expr = parseExpr(comprExpr[0], parsedModel);
    quantifier->setExpression(expr.second);
    for (auto& varName : variablesAddedToScope) {
        parsedModel.namedExprs.erase(varName);
    }
    return expr.first;
}

template <typename ContainerReturnType, typename ContainerDomainPtrType>
pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> buildQuant(
    json& comprExpr, ExprRef<ContainerReturnType>& container,
    ContainerDomainPtrType&& domain, ParsedModel& parsedModel) {
    auto quantifier = make_shared<Quantifier<ContainerReturnType>>(container);
    AnyDomainRef innerDomain =
        addExprToQuantifier(comprExpr, domain, quantifier, parsedModel);
    return make_pair(fakeSequenceDomain(innerDomain),
                     ExprRef<SequenceView>(quantifier));
}

pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> parseComprehension(
    json& comprExpr, ParsedModel& parsedModel) {
    auto& generatorExpr = comprExpr[1][0]["Generator"]["GenInExpr"];

    auto errorHandler = [&](auto &&)
        -> pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> {
        cerr << "Error, not yet handling quantifier for this type: "
             << generatorExpr << endl;
        abort();
    };
    auto domainContainerPair = parseExpr(generatorExpr[1], parsedModel);
    return mpark::visit(
        overloaded(
            [&](ExprRef<SetView>& set)
                -> pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> {
                auto& domain = mpark::get<shared_ptr<SetDomain>>(
                    domainContainerPair.first);
                return buildQuant(comprExpr, set, domain, parsedModel);
            },
            [&](ExprRef<MSetView>& mSet)
                -> pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> {
                auto& domain = mpark::get<shared_ptr<MSetDomain>>(
                    domainContainerPair.first);
                return buildQuant(comprExpr, mSet, domain, parsedModel);
            },
            move(errorHandler)),
        domainContainerPair.second);
}

pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> parseSequenceLikeExpr(
    json& expr, ParsedModel& parsedModel) {
    if (expr.count("AbstractLiteral")) {
        if (expr["AbstractLiteral"].count("AbsLitMatrix")) {
            return parseConstantMatrix(expr["AbstractLiteral"]["AbsLitMatrix"],
                                       parsedModel);
        }
    } else if (expr.count("Comprehension")) {
        return parseComprehension(expr["Comprehension"], parsedModel);
    }
    cerr << "Not sure how to parse this type within the context of an "
            "argument list, expected constant matrix or quantifier.\n"
         << expr << endl;
    abort();
}

template <typename View, typename Op, typename Domain>
auto makeVaradicOpParser(const Domain& domain) {
    return [&](json& essenceExpr,
               ParsedModel& parsedModel) -> pair<AnyDomainRef, AnyExprRef> {
        return make_pair(
            domain,
            OpMaker<Op>::make(
                parseSequenceLikeExpr(essenceExpr, parsedModel).second));
    };
}

pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseExpr(
    json& essenceExpr, ParsedModel& parsedModel) {
    if (essenceExpr.count("Op")) {
        auto boolExprPair = stringMatch<ParseExprFunction>(
            {{"MkOpEq", parseOpEq},
             {"MkOpMod", parseOpMod},
             {"MkOpTwoBars", parseOpTwoBars},
             {"MkOpSubsetEq", parseOpSubsetEq},
             {"MkOpAnd", makeVaradicOpParser<BoolView, OpAnd>(fakeBoolDomain)},
             {"MkOpOr", makeVaradicOpParser<BoolView, OpOr>(fakeBoolDomain)},
             {"MkOpSum", makeVaradicOpParser<IntView, OpSum>(fakeIntDomain)},
             {"MkOpProduct",
              makeVaradicOpParser<IntView, OpProd>(fakeIntDomain)},
             {"MkOpRelationProj", parseOpRelationProj}

            },
            make_pair(AnyDomainRef(fakeIntDomain),
                      AnyExprRef(ExprRef<IntView>(nullptr))),
            essenceExpr["Op"], parsedModel);
        if (boolExprPair.first) {
            return boolExprPair;
        }
    }

    auto boolValuePair = tryParseValue(essenceExpr, parsedModel);
    if (boolValuePair.first) {
        return boolValuePair;
    }
    return make_pair(false,
                     make_pair(fakeBoolDomain, ExprRef<BoolView>(nullptr)));
}

void handleLettingDeclaration(json& lettingArray, ParsedModel& parsedModel) {
    string lettingName = lettingArray[0]["Name"];
    auto boolValuePair = tryParseValue(lettingArray[1], parsedModel);
    if (boolValuePair.first) {
        parsedModel.namedExprs.emplace(lettingName, boolValuePair.second);
        return;
    }
    if (lettingArray[1].count("Domain")) {
        auto domain = parseDomain(lettingArray[1]["Domain"], parsedModel);
        parsedModel.domainLettings.emplace(lettingName, domain);
        return;
    }
    cerr << "Not sure how to parse this letting: " << lettingArray << endl;
}

void handleFindDeclaration(json& findArray, ParsedModel& parsedModel) {
    string findName = findArray[1]["Name"];
    auto findDomain = parseDomain(findArray[2], parsedModel);
    mpark::visit(
        [&](auto& domainImpl) {
            parsedModel.namedExprs.emplace(
                findName,
                make_pair(domainImpl,
                          parsedModel.builder->addVariable(findName, domainImpl)
                              .asExpr()));
        },
        findDomain);
}

void parseExprs(json& suchThat, ParsedModel& parsedModel) {
    for (auto& op : suchThat) {
        ExprRef<BoolView> constraint =
            expect<BoolView>(parseExpr(op, parsedModel).second, [&](auto&&) {
                cerr << "Expected Bool returning constraint within such that: "
                     << op << endl;
            });
        parsedModel.builder->addConstraint(constraint);
    }
}

inline void parseObjective(json& objExpr, ParsedModel& parsedModel) {
    string modeStr = objExpr[0];
    OptimiseMode mode = (modeStr == "Minimising") ? OptimiseMode::MINIMISE
                                                  : OptimiseMode::MAXIMISE;

    ExprRef<IntView> objConstraint =
        expect<IntView>(parseExpr(objExpr[1], parsedModel).second, [&](auto&&) {
            cerr << "Expected int returning expression for objective: "
                 << objExpr << endl;
        });
    parsedModel.builder->setObjective(mode, objConstraint);
}

ParsedModel parseModelFromJson(istream& is) {
    json j;
    is >> j;
    ParsedModel parsedModel;
    for (auto& statement : j["mStatements"]) {
        if (statement.count("Declaration")) {
            auto& declaration = statement["Declaration"];
            if (declaration.count("Letting")) {
                handleLettingDeclaration(declaration["Letting"], parsedModel);
            } else if (declaration.count("FindOrGiven") &&
                       declaration["FindOrGiven"][0] == "Find") {
                handleFindDeclaration(declaration["FindOrGiven"], parsedModel);
            }
        } else if (statement.count("SuchThat")) {
            parseExprs(statement["SuchThat"], parsedModel);
        } else if (statement.count("Objective")) {
            parseObjective(statement["Objective"], parsedModel);
        }
    }
    return parsedModel;
}
