#include "jsonModelParser.h"
#include <functional>
#include <iostream>
#include <json.hpp>
#include <unordered_map>
#include <utility>
#include "common/common.h"
#include "operators/opPowerSet.h"
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

shared_ptr<FunctionDomain> fakeFunctionDomain(AnyDomainRef d1,
                                              AnyDomainRef d2) {
    return make_shared<FunctionDomain>(JectivityAttr::NONE, PartialAttr::TOTAL,
                                       move(d1), move(d2));
}

shared_ptr<TupleDomain> fakeTupleDomain(std::vector<AnyDomainRef> domains) {
    return make_shared<TupleDomain>(move(domains));
}

ParsedModel::ParsedModel() : builder(make_unique<ModelBuilder>()) {}
typedef function<pair<AnyDomainRef, AnyExprRef>(json&, ParsedModel&)>
    ParseExprFunction;
typedef function<pair<AnyDomainRef, AnyExprRef>(json&, ParsedModel&)>
    ParseDomainGeneratorFunction;
typedef function<AnyDomainRef(json&, ParsedModel&)> ParseDomainFunction;

template <typename RetType, typename Constraint, typename Func>
ExprRef<RetType> expect(Constraint&& constraint, Func&& func);
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
pair<AnyDomainRef, AnyExprRef> parseDomainGenerator(json& domainExpr,
                                                    ParsedModel& parsedModel);

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

AnyValRef toValRef(const AnyExprRef& op) {
    return mpark::visit(
        [&](auto& ref) -> AnyValRef {
            typedef typename AssociatedValueType<viewType(ref)>::type ValType;
            auto val = make<ValType>();
            val.asExpr()->view() = ref->view();
            return val;
        },
        op);
}

Int parseValueAsInt(json& essenceExpr, ParsedModel& parsedModel,
                    const string& errorMessage) {
    auto intExpr = expect<IntView>(
        parseExpr(essenceExpr, parsedModel).second, [&](auto&&) {
            cerr << "Error: Expected int expression " << errorMessage << endl;
            cerr << essenceExpr << endl;
        });
    intExpr->evaluate();
    return intExpr->view().value;
}

Int parseValueAsInt(AnyExprRef expr, const string& errorMessage) {
    auto intExpr = expect<IntView>(expr, [&](auto&&) {
        cerr << "Error: Expected int expression " << errorMessage << endl;
    });
    intExpr->evaluate();
    return intExpr->view().value;
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

pair<shared_ptr<SetDomain>, ExprRef<SetView>> parseOpSetLit(
    json& setExpr, ParsedModel& parsedModel) {
    if (setExpr.size() == 0) {
        cerr << "Not sure how to work out type of empty setyet, will handle "
                "this later.";
        todoImpl();
    }
    AnyExprVec newSetMembers;
    AnyDomainRef innerDomain = fakeIntDomain;
    bool constant = true;
    bool first = true;
    for (auto& elementExpr : setExpr) {
        auto expr = parseExpr(elementExpr, parsedModel);
        if (first) {
            innerDomain = expr.first;
        }
        mpark::visit(
            [&](auto& member) {
                if (first) {
                    newSetMembers.emplace<ExprRefVec<viewType(member)>>();
                }
                constant &= member->isConstant();

                mpark::get<ExprRefVec<viewType(member)>>(newSetMembers)
                    .emplace_back(move(member));
            },
            expr.second);
        first = false;
    }
    auto set = OpMaker<OpSetLit>::make(move(newSetMembers));
    set->setConstant(constant);
    return make_pair(fakeSetDomain(innerDomain), set);
}

void matchType(AnyExprRef& expr, AnyExprVec& vec) {
    mpark::visit([&](auto& expr) { vec.emplace<ExprRefVec<viewType(expr)>>(); },
                 expr);
}

void append(AnyExprVec& vec, AnyExprRef& expr) {
    mpark::visit(
        [&](auto& vec) {
            vec.emplace_back(mpark::get<ExprRef<viewType(vec)>>(expr));
        },
        vec);
}
void updateDimensions(AnyExprRef preImage, DimensionVec& dimVec) {
    string errorMessage =
        "\nOnly supporting functions from int or tuple of int.";
    mpark::visit(
        overloaded(
            [&](ExprRef<IntView>& expr) {
                Int v = expr->view().value;
                if (dimVec.empty()) {
                    dimVec.emplace_back(v, v);
                } else {
                    dimVec.front().lower = min(dimVec.front().lower, v);
                    dimVec.front().upper = max(dimVec.front().upper, v);
                }
            },
            [&](ExprRef<TupleView>& tuple) {
                if (dimVec.empty()) {
                    for (auto& member : tuple->view().members) {
                        Int v = parseValueAsInt(member, errorMessage);
                        dimVec.emplace_back(v, v);
                    }
                } else {
                    debug_code(
                        assert(tuple->view().members.size() == dimVec.size()));
                    for (size_t i = 0; i < tuple->view().members.size(); i++) {
                        Int v = parseValueAsInt(tuple->view().members[i],
                                                errorMessage);
                        auto& dim = dimVec[i];
                        dim.lower = min(dim.lower, v);
                        dim.upper = max(dim.upper, v);
                    }
                }
            },
            [&](auto&) {
                cerr << "Error: " << errorMessage << endl;
                abort();
            }),
        preImage);
}

pair<AnyDomainRef, AnyDomainRef> parseDefinedAndRange(json& functionDomainExpr,
                                                      ParsedModel& parsedModel,
                                                      AnyExprVec& defined,
                                                      AnyExprVec& range,
                                                      DimensionVec& dimVec) {
    pair<AnyDomainRef, AnyDomainRef> functionDomain(fakeIntDomain,
                                                    fakeIntDomain);
    bool first = true;
    for (auto& mapping : functionDomainExpr) {
        auto preImage = parseExpr(mapping[0], parsedModel);
        auto image = parseExpr(mapping[1], parsedModel);
        if (first) {
            first = false;
            matchType(preImage.second, defined);
            matchType(image.second, range);
            functionDomain = make_pair(preImage.first, image.first);
        }
        append(defined, preImage.second);
        append(range, image.second);
        updateDimensions(preImage.second, dimVec);
    }
    return functionDomain;
}
// will not work with expressions
pair<shared_ptr<FunctionDomain>, ExprRef<FunctionView>> parseConstantFunction(
    json& functionDomainExpr, ParsedModel& parsedModel) {
    DimensionVec dimVec;
    AnyExprVec defined;
    AnyExprVec range;
    pair<AnyDomainRef, AnyDomainRef> functionDomain = parseDefinedAndRange(
        functionDomainExpr, parsedModel, defined, range, dimVec);
    auto function = make<FunctionValue>();
    function->setConstant(true);
    mpark::visit(
        [&](auto& defined, auto& range) {
            if (range.empty()) {
                cerr << "Not handling empty functions yet, sorry.\n";
                cerr << functionDomainExpr << endl;
                abort();
            }
            function->resetDimensions<viewType(range)>(dimVec);
            for (size_t i = 0; i < defined.size(); i++) {
                if (!defined[i]->isConstant() || !range[i]->isConstant()) {
                    cerr << "At the moment, only handling constant function "
                            "literals.\n";
                    abort();
                }
                auto boolIndexPair =
                    function->domainToIndex(defined[i]->view());
                assert(boolIndexPair.first);
                function->assignImage(boolIndexPair.second,
                                      assumeAsValue(range[i]));
            }
        },
        defined, range);
    return make_pair(
        fakeFunctionDomain(functionDomain.first, functionDomain.second),
        function.asExpr());
}
template <typename View,
          typename Value = typename AssociatedValueType<View>::type>
inline ValRef<Value> getIfConstValue(ExprRef<View>& expr) {
    Value* value = dynamic_cast<Value*>(&(*expr));
    if (value && value->container == &constantPool) {
        return assumeAsValue(expr);
    } else {
        return ValRef<Value>(nullptr);
    }
}

pair<shared_ptr<TupleDomain>, ExprRef<TupleView>> parseOpTupleLit(
    json& tupleExpr, ParsedModel& parsedModel) {
    bool constant = true;
    vector<AnyDomainRef> tupleMemberDomains;
    vector<AnyExprRef> tupleMembers;
    for (auto& memberExpr : tupleExpr) {
        auto member = parseExpr(memberExpr, parsedModel);
        tupleMemberDomains.emplace_back(member.first);
        tupleMembers.emplace_back(member.second);
        mpark::visit([&](auto& member) { constant &= member->isConstant(); },
                     member.second);
    }
    auto tuple = OpMaker<OpTupleLit>::make(move(tupleMembers));
    tuple->setConstant(constant);
    return make_pair(fakeTupleDomain(move(tupleMemberDomains)), tuple);
}
pair<shared_ptr<IntDomain>, ExprRef<IntView>> parseConstantInt(json& intExpr,
                                                               ParsedModel&) {
    auto val = make<IntValue>();
    val->value = intExpr;
    val->setConstant(true);
    return make_pair(fakeIntDomain, val.asExpr());
}

pair<shared_ptr<BoolDomain>, ExprRef<BoolView>> parseConstantBool(
    json& boolExpr, ParsedModel&) {
    auto val = make<BoolValue>();
    val->violation = (bool(boolExpr)) ? 0 : 1;
    val->setConstant(true);
    return make_pair(fakeBoolDomain, val.asExpr());
}

pair<AnyDomainRef, AnyExprRef> parseValueReference(json& essenceReference,
                                                   ParsedModel& parsedModel) {
    string referenceName = essenceReference[0]["Name"];
    if (parsedModel.namedExprs.count(referenceName)) {
        return parsedModel.namedExprs.at(referenceName);
    } else {
        cerr << "Found reference to value with name \"" << referenceName
             << "\" but this does not appear to be in scope.\n";
        abort();
    }
}

shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel) {
    vector<pair<Int, Int>> ranges;
    string errorMessage = "Within context of int domain";

    for (auto& rangeExpr : intDomainExpr) {
        Int from, to;

        if (rangeExpr.count("RangeBounded")) {
            from = parseValueAsInt(rangeExpr["RangeBounded"][0], parsedModel,
                                   errorMessage);
            to = parseValueAsInt(rangeExpr["RangeBounded"][1], parsedModel,
                                 errorMessage);
        } else if (rangeExpr.count("RangeSingle")) {
            from = parseValueAsInt(rangeExpr["RangeSingle"], parsedModel,
                                   errorMessage);
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
    string errorMessage = "within size attribute";
    if (sizeAttrExpr.count("SizeAttr_None")) {
        return noSize();
    } else if (sizeAttrExpr.count("SizeAttr_MinSize")) {
        return minSize(parseValueAsInt(sizeAttrExpr["SizeAttr_MinSize"],
                                       parsedModel, errorMessage));
    } else if (sizeAttrExpr.count("SizeAttr_MaxSize")) {
        return maxSize(parseValueAsInt(sizeAttrExpr["SizeAttr_MaxSize"],
                                       parsedModel, errorMessage));
    } else if (sizeAttrExpr.count("SizeAttr_Size")) {
        return exactSize(parseValueAsInt(sizeAttrExpr["SizeAttr_Size"],
                                         parsedModel, errorMessage));
    } else if (sizeAttrExpr.count("SizeAttr_MinMaxSize")) {
        auto& sizeRangeExpr = sizeAttrExpr["SizeAttr_MinMaxSize"];
        return sizeRange(
            parseValueAsInt(sizeRangeExpr[0], parsedModel, errorMessage),
            parseValueAsInt(sizeRangeExpr[1], parsedModel, errorMessage));
    } else {
        cerr << "Could not parse this as a size attribute: " << sizeAttrExpr
             << endl;
        abort();
    }
}

PartialAttr parsePartialAttr(json& partialAttrExpr) {
    if (partialAttrExpr == "PartialityAttr_Partial") {
        return PartialAttr::PARTIAL;
    } else if (partialAttrExpr == "PartialityAttr_Total") {
        return PartialAttr::TOTAL;
    } else {
        cerr << "Error: unknown partiality attribute: " << partialAttrExpr
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
    if (sequenceDomainExpr[1][1] == "JectivityAttr_None") {
        return make_shared<SequenceDomain>(
            sizeAttr, parseDomain(sequenceDomainExpr[2], parsedModel));
    } else if (sequenceDomainExpr[1][1] == "JectivityAttr_Injective") {
        return make_shared<SequenceDomain>(
            sizeAttr, parseDomain(sequenceDomainExpr[2], parsedModel), true);
    } else {
        cerr << "Not sure what this attribute for domain sequence is:\n"
             << sequenceDomainExpr[1][1] << endl;
        abort();
    }
}

shared_ptr<FunctionDomain> parseDomainFunction(json& functionDomainExpr,
                                               ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(functionDomainExpr[1][0], parsedModel);

    if (sizeAttr.sizeType != SizeAttr::SizeAttrType::NO_SIZE) {
        cerr << "Do not support/understand function with size attribute.\nn";
        abort();
    }
    PartialAttr partialAttr = parsePartialAttr(functionDomainExpr[1][1]);
    if (partialAttr != PartialAttr::TOTAL) {
        cerr << "Error: currently only support total functions.\n";
        abort();
    }
    if (functionDomainExpr[1][2] != "JectivityAttr_None") {
        cerr << "Error, not supporting jectivity attributes on functions at "
                "the moment.\n";
        abort();
    }
    return make_shared<FunctionDomain>(
        JectivityAttr::NONE, partialAttr,
        parseDomain(functionDomainExpr[2], parsedModel),
        parseDomain(functionDomainExpr[3], parsedModel));
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
         {"DomainFunction", parseDomainFunction},

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

pair<AnyDomainRef, AnyExprRef> parseOpMinus(json& minusExpr,
                                            ParsedModel& parsedModel) {
    string errorMessage = "Expected int returning expression within Op minus: ";
    ExprRef<IntView> left =
        expect<IntView>(parseExpr(minusExpr[0], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << minusExpr[0]; });
    ExprRef<IntView> right =
        expect<IntView>(parseExpr(minusExpr[1], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << minusExpr[1]; });
    return make_pair(fakeIntDomain,
                     OpMaker<OpMinus>::make(move(left), move(right)));
}

pair<AnyDomainRef, AnyExprRef> parseOpPower(json& powerExpr,
                                            ParsedModel& parsedModel) {
    string errorMessage = "Expected int returning expression within Op power: ";
    ExprRef<IntView> left =
        expect<IntView>(parseExpr(powerExpr[0], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << powerExpr[0]; });
    ExprRef<IntView> right =
        expect<IntView>(parseExpr(powerExpr[1], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << powerExpr[1]; });
    return make_pair(fakeIntDomain,
                     OpMaker<OpPower>::make(move(left), move(right)));
}

pair<AnyDomainRef, AnyExprRef> parseOpIn(json& inExpr,
                                         ParsedModel& parsedModel) {
    AnyExprRef expr = parseExpr(inExpr[0], parsedModel).second;
    auto setOperand =
        expect<SetView>(parseExpr(inExpr[1], parsedModel).second, [&](auto&&) {
            cerr << "OpIn expects a set on the right.\n" << inExpr;
        });
    return make_pair(fakeBoolDomain,
                     OpMaker<OpIn>::make(move(expr), move(setOperand)));
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

pair<AnyDomainRef, AnyExprRef> parseOpPowerSet(json& powerSetExpr,
                                               ParsedModel& parsedModel) {
    string errorMessage =
        "Expected set returning expression within Op PowerSet: ";
    auto parsedExpr = parseExpr(powerSetExpr, parsedModel);
    auto operand = expect<SetView>(parsedExpr.second, [&](auto&&) {
        cerr << errorMessage << powerSetExpr << endl;
    });
    auto& domain = mpark::get<shared_ptr<SetDomain>>(parsedExpr.first);
    return make_pair(fakeSetDomain(fakeSetDomain(domain->inner)),
                     OpMaker<OpPowerSet>::make(move(operand)));
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

pair<AnyDomainRef, AnyExprRef> parseOpFunctionImage(
    AnyDomainRef& innerDomain, ExprRef<FunctionView>& function,
    json& preImageExpr, ParsedModel& parsedModel) {
    auto preImage = parseExpr(preImageExpr[0], parsedModel).second;
    return mpark::visit(
        [&](auto& innerDomain) -> pair<AnyDomainRef, AnyExprRef> {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<
                typename AssociatedValueType<InnerDomainType>::type>::type View;
            return make_pair(innerDomain, OpMaker<OpFunctionImage<View>>::make(
                                              function, preImage));
        },
        innerDomain);
}

pair<AnyDomainRef, AnyExprRef> parseOpTupleIndex(
    shared_ptr<TupleDomain>& tupleDomain, ExprRef<TupleView>& tuple,
    json& indexExpr, ParsedModel& parsedModel) {
    string errorMessage = "within tuple index expression.";
    UInt index = parseValueAsInt(indexExpr[0], parsedModel, errorMessage) - 1;
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
            [&](ExprRef<FunctionView>& function)
                -> pair<AnyDomainRef, AnyExprRef> {
                auto& innerDomain =
                    mpark::get<shared_ptr<FunctionDomain>>(leftOperand.first)
                        ->to;
                return parseOpFunctionImage(innerDomain, function,
                                            operandsExpr[1], parsedModel);
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

pair<AnyDomainRef, AnyExprRef> parseOpNotEq(json& operandsExpr,
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
            typedef viewType(left) View;
            return make_pair(fakeBoolDomain,
                             OpMaker<OpNotEq<View>>::make(
                                 left, expect<View>(right, errorHandler)));
        },
        left);
}
pair<AnyDomainRef, AnyExprRef> parseOpCatchUndef(json& operandsExpr,
                                                 ParsedModel& parsedModel) {
    auto domainExprPair = parseExpr(operandsExpr[0], parsedModel);
    auto& domain = domainExprPair.first;
    AnyExprRef& left = domainExprPair.second;
    AnyExprRef right = parseExpr(operandsExpr[1], parsedModel).second;
    return mpark::visit(
        [&](auto& left) -> pair<AnyDomainRef, AnyExprRef> {
            auto errorHandler = [&](auto&&) {
                cerr << "Expected right operand to be of same type as left, "
                        "i.e. "
                     << TypeAsString<typename AssociatedValueType<viewType(
                            left)>::type>::value
                     << endl;
            };
            typedef viewType(left) View;
            return make_pair(domain,
                             OpMaker<OpCatchUndef<View>>::make(
                                 left, expect<View>(right, errorHandler)));
        },
        left);
}
template <bool flipped = false>
pair<shared_ptr<BoolDomain>, ExprRef<BoolView>> parseOpLess(
    json& expr, ParsedModel& parsedModel) {
    auto errorFunc = [&](auto&&) {
        cerr << "Expected int within op less\n" << expr << endl;
    };
    auto left =
        expect<IntView>(parseExpr(expr[0], parsedModel).second, errorFunc);
    auto right =
        expect<IntView>(parseExpr(expr[1], parsedModel).second, errorFunc);
    if (flipped) {
        swap(left, right);
    }

    return make_pair(fakeBoolDomain, OpMaker<OpLess>::make(left, right));
}

pair<shared_ptr<BoolDomain>, ExprRef<BoolView>> parseOpImplies(
    json& expr, ParsedModel& parsedModel) {
    auto errorFunc = [&](auto&&) {
        cerr << "Expected bool within OpImplies less\n" << expr << endl;
    };
    auto left =
        expect<BoolView>(parseExpr(expr[0], parsedModel).second, errorFunc);
    auto right =
        expect<BoolView>(parseExpr(expr[1], parsedModel).second, errorFunc);
    return make_pair(fakeBoolDomain, OpMaker<OpImplies>::make(left, right));
}

template <bool flipped = false>
pair<shared_ptr<BoolDomain>, ExprRef<BoolView>> parseOpLessEq(
    json& expr, ParsedModel& parsedModel) {
    auto errorFunc = [&](auto&&) {
        cerr << "Expected int within op less eq\n" << expr << endl;
    };
    auto left =
        expect<IntView>(parseExpr(expr[0], parsedModel).second, errorFunc);
    auto right =
        expect<IntView>(parseExpr(expr[1], parsedModel).second, errorFunc);
    if (flipped) {
        swap(left, right);
    }
    return make_pair(fakeBoolDomain, OpMaker<OpLessEq>::make(left, right));
}
pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> parseOpSequenceLit(
    json& matrixExpr, ParsedModel& parsedModel) {
    if (matrixExpr[1].size() == 0) {
        cerr << "Not sure how to work out type of empty matrix or sequence "
                "yet, will handle "
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
             << " members to be present in tuple.  However, it appears "
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
void extractPatternMatchAndAddExprsToScope(json&, const shared_ptr<DomainType>&,
                                           ExprRef<ViewType>&, ParsedModel&,
                                           vector<string>&) {
    shouldNotBeCalledPanic;
}

template <typename DomainType, typename ViewType,
          EnableIfDomainMatchesView<DomainType, ViewType> = 0>
void extractPatternMatchAndAddExprsToScope(
    json& patternExpr, const shared_ptr<DomainType>& domain,
    ExprRef<ViewType>& expr, ParsedModel& parsedModel,
    vector<string>& variablesAddedToScope) {
    if (patternExpr.count("Single")) {
        string name = patternExpr["Single"]["Name"];
        variablesAddedToScope.emplace_back(name);
        parsedModel.namedExprs.emplace(variablesAddedToScope.back(),
                                       make_pair(domain, expr));
    } else if (patternExpr.count("AbsPatTuple")) {
        overloaded(
            [&](auto&, auto&) {
                cerr << "Error, Found tuple pattern, but in this context "
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
                            extractPatternMatchAndAddExprsToScope(
                                tupleMatchExpr[i], innerDomain, tupleIndexExpr,
                                parsedModel, variablesAddedToScope);
                        },
                        domain->inners[i]);
                }
            })(domain, expr);
    } else if (patternExpr.count("AbsPatSet")) {
        overloaded(
            [&](auto&, auto&) {
                cerr << "Error, found set pattern, but did not expect a set "
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
                            extractPatternMatchAndAddExprsToScope(
                                setMatchExpr[i], innerDomain, setIndexExpr,
                                parsedModel, variablesAddedToScope);
                        }
                    },
                    domain->inner);
            })(domain, expr);
    }
}

template <typename ContainerDomainType, typename Quantifier>
AnyDomainRef addExprToQuantifier(
    json& comprExpr, shared_ptr<ContainerDomainType>& containerDomain,
    Quantifier& quantifier, ParsedModel& parsedModel) {
    json& generatorParent = comprExpr[1][0]["Generator"];
    json& generatorExpr = (generatorParent.count("GenInExpr"))
                              ? generatorParent["GenInExpr"]
                              : generatorParent["GenDomainNoRepr"];
    vector<string> variablesAddedToScope;
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
            overloaded(
                [&](shared_ptr<SetDomain>&) {
                    OpPowerSet* powerSetTest =
                        dynamic_cast<OpPowerSet*>(&(*(quantifier->container)));
                    if (powerSetTest != NULL) {
                        auto iterRef =
                            quantifier->template newIterRef<SetView>();
                        auto iterDomain = innerDomain;
                        ExprRef<SetView> iter(iterRef);
                        extractPatternMatchAndAddExprsToScope(
                            generatorExpr[0], iterDomain, iter, parsedModel,
                            variablesAddedToScope);
                        powerSetTest->sizeLimit =
                            generatorExpr[0]["AbsPatSet"].size();
                    } else {
                        ExprRef<InnerViewType> iter(
                            quantifier->template newIterRef<InnerViewType>());
                        extractPatternMatchAndAddExprsToScope(
                            generatorExpr[0], innerDomain, iter, parsedModel,
                            variablesAddedToScope);
                    }
                },
                [&](shared_ptr<MSetDomain>&) {
                    ExprRef<InnerViewType> iter(
                        quantifier->template newIterRef<InnerViewType>());
                    extractPatternMatchAndAddExprsToScope(
                        generatorExpr[0], innerDomain, iter, parsedModel,
                        variablesAddedToScope);
                },
                [&](shared_ptr<SequenceDomain>&) {
                    auto iterRef = quantifier->template newIterRef<TupleView>();
                    if (generatorParent.count("GenDomainNoRepr")) {
                        auto iter = OpMaker<OpTupleIndex<InnerViewType>>::make(
                            iterRef, 1);
                        extractPatternMatchAndAddExprsToScope(
                            generatorExpr[0], innerDomain, iter, parsedModel,
                            variablesAddedToScope);
                    } else {
                        auto iterDomain =
                            fakeTupleDomain({fakeIntDomain, innerDomain});
                        ExprRef<TupleView> iter(iterRef);
                        extractPatternMatchAndAddExprsToScope(
                            generatorExpr[0], iterDomain, iter, parsedModel,
                            variablesAddedToScope);
                    }
                },
                [&](shared_ptr<FunctionDomain>& functionDomain) {
                    ExprRef<TupleView> iter(
                        quantifier->template newIterRef<TupleView>());
                    auto iterDomain =
                        fakeTupleDomain({functionDomain->from, innerDomain});
                    extractPatternMatchAndAddExprsToScope(
                        generatorExpr[0], iterDomain, iter, parsedModel,
                        variablesAddedToScope);

                },
                [&](auto&) {
                    cerr << "no support for this type\n";
                    abort();

                })(containerDomain);
        },
        innerDomain);
    auto expr = parseExpr(comprExpr[0], parsedModel);
    quantifier->setExpression(expr.second);
    vector<ExprRef<BoolView>> conditions;
    for (auto& expr : comprExpr[1]) {
        if (expr.count("Condition")) {
            auto parsedCondition = expect<BoolView>(
                parseExpr(expr["Condition"], parsedModel).second, [&](auto&&) {
                    cerr << "Conitions for quantifiers must be bool "
                            "returning.\n";
                });
            conditions.emplace_back(parsedCondition);
        }
    }
    if (conditions.size() > 1) {
        quantifier->setCondition(OpMaker<OpAnd>::make(
            OpMaker<OpSequenceLit>::make(move(conditions))));
    } else if (conditions.size() == 1) {
        quantifier->setCondition(conditions[0]);
    }
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
    auto& generatorParent = comprExpr[1][0]["Generator"];
    json& generatorExpr = (generatorParent.count("GenInExpr"))
                              ? generatorParent["GenInExpr"]
                              : generatorParent["GenDomainNoRepr"];

    auto errorHandler = [&](auto &&)
        -> pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> {
        cerr << "Error, not yet handling quantifier for this type: "
             << generatorExpr << endl;
        abort();
    };
    auto domainContainerPair =
        (generatorParent.count("GenInExpr"))
            ? parseExpr(generatorExpr[1], parsedModel)
            : parseDomainGenerator(generatorExpr[1], parsedModel);
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
            [&](ExprRef<SequenceView>& sequence)
                -> pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> {
                auto& domain = mpark::get<shared_ptr<SequenceDomain>>(
                    domainContainerPair.first);
                return buildQuant(comprExpr, sequence, domain, parsedModel);
            },
            [&](ExprRef<FunctionView>& function)
                -> pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> {
                auto& domain = mpark::get<shared_ptr<FunctionDomain>>(
                    domainContainerPair.first);
                return buildQuant(comprExpr, function, domain, parsedModel);
            },
            move(errorHandler)),
        domainContainerPair.second);
}

template <typename View, typename Op, typename Domain>
auto makeVaradicOpParser(const Domain& domain) {
    return [&](json& essenceExpr,
               ParsedModel& parsedModel) -> pair<AnyDomainRef, AnyExprRef> {
        auto sequence = expect<SequenceView>(
            parseExpr(essenceExpr, parsedModel).second, [&](auto&&) {
                cerr << "In the context of parsing arguments to a function, "
                        "requires matrix/sequence/quantifier.\n"
                     << essenceExpr << endl;
            });
        return make_pair(domain, OpMaker<Op>::make(sequence));
    };
}

pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> quantifyOver(
    shared_ptr<SetDomain>& domain, ExprRef<SetView>& expr) {
    auto quant = make_shared<Quantifier<SetView>>(expr);
    return mpark::visit(
        [&](auto& innerDomain)
            -> pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> {
            typedef typename AssociatedValueType<typename BaseType<decltype(
                innerDomain)>::element_type>::type InnerValueType;
            typedef
                typename AssociatedViewType<InnerValueType>::type InnerViewType;
            quant->setExpression(
                ExprRef<InnerViewType>(quant->newIterRef<InnerViewType>()));
            return make_pair(fakeSequenceDomain(innerDomain), quant);
        },
        domain->inner);
}

template <typename Func>
pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> toSequence(
    pair<AnyDomainRef, AnyExprRef> parsedExpr, Func&& errorFunc) {
    return mpark::visit(
        overloaded(
            [&](shared_ptr<SequenceDomain>& domain) {
                return make_pair(domain, mpark::get<ExprRef<SequenceView>>(
                                             parsedExpr.second));
            },
            [&](shared_ptr<SetDomain>& domain) {
                return quantifyOver(
                    domain, mpark::get<ExprRef<SetView>>(parsedExpr.second));
            },
            [&](auto& domain)
                -> pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>> {
                typedef typename AssociatedValueType<typename BaseType<decltype(
                    domain)>::element_type>::type ValueType;
                cerr << "Unexpected type: " << TypeAsString<ValueType>::value
                     << endl;
                errorFunc();
                abort();
            }),
        parsedExpr.first);
}

template <bool minMode>
pair<shared_ptr<IntDomain>, ExprRef<IntView>> parseOpMinMax(
    json& operandExpr, ParsedModel& parsedModel) {
    string message = "Only supporting min/max over sequence of ints.\n";
    auto parsedExpr = toSequence(parseExpr(operandExpr, parsedModel), [&]() {
        cerr << "in context of op min/max.\n" << operandExpr << endl;
    });
    shared_ptr<IntDomain>* intDomainTest =
        mpark::get_if<shared_ptr<IntDomain>>(&(parsedExpr.first->inner));
    if (intDomainTest) {
        typedef OpMinMax<minMode> Op;
        return make_pair(*intDomainTest, OpMaker<Op>::make(parsedExpr.second));
    }
    cerr << "Only supporting min/max over ints.\n";
    cerr << operandExpr << endl;
    abort();
}

pair<shared_ptr<IntDomain>, ExprRef<IntView>> parseOpToInt(
    json& expr, ParsedModel& parsedModel) {
    auto boolExpr = expect<BoolView>(
        parseExpr(expr, parsedModel).second,
        [&](auto&) { cerr << "OpToInt requires a bool operand.\n"; });
    return make_pair(fakeIntDomain, OpMaker<OpToInt>::make(boolExpr));
}

pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseExpr(
    json& essenceExpr, ParsedModel& parsedModel) {
    if (essenceExpr.count("Constant")) {
        return tryParseExpr(essenceExpr["Constant"], parsedModel);
    } else if (essenceExpr.count("ConstantAbstract")) {
        return tryParseExpr(essenceExpr["ConstantAbstract"], parsedModel);
    } else if (essenceExpr.count("AbstractLiteral")) {
        return tryParseExpr(essenceExpr["AbstractLiteral"], parsedModel);
    } else if (essenceExpr.count("Op")) {
        return tryParseExpr(essenceExpr["Op"], parsedModel);
    } else {
        return stringMatch<ParseExprFunction>(
            {{"ConstantInt", parseConstantInt},
             {"ConstantBool", parseConstantBool},
             {"AbsLitSet", parseOpSetLit},
             {"AbsLitMatrix", parseOpSequenceLit},
             {"AbsLitFunction", parseConstantFunction},
             {"AbsLitTuple", parseOpTupleLit},
             {"Reference", parseValueReference},
             {"Comprehension", parseComprehension},
             {"MkOpEq", parseOpEq},
             {"MkOpLt", parseOpLess<false>},
             {"MkOpLeq", parseOpLessEq<false>},
             {"MkOpGt", parseOpLess<true>},
             {"MkOpGeq", parseOpLessEq<true>},
             {"MkOpNeq", parseOpNotEq},
             {"MkOpImply", parseOpImplies},
             {"MkOpCatchUndef", parseOpCatchUndef},
             {"MkOpIn", parseOpIn},
             {"MkOpMod", parseOpMod},
             {"MkOpMinus", parseOpMinus},
             {"MkOpPow", parseOpPower},
             {"MkOpTwoBars", parseOpTwoBars},
             {"MkOpPowerSet", parseOpPowerSet},
             {"MkOpSubsetEq", parseOpSubsetEq},
             {"MkOpAnd", makeVaradicOpParser<BoolView, OpAnd>(fakeBoolDomain)},
             {"MkOpOr", makeVaradicOpParser<BoolView, OpOr>(fakeBoolDomain)},
             {"MkOpSum", makeVaradicOpParser<IntView, OpSum>(fakeIntDomain)},
             {"MkOpProduct",
              makeVaradicOpParser<IntView, OpProd>(fakeIntDomain)},
             {"MkOpRelationProj", parseOpRelationProj},
             {"MkOpMin", parseOpMinMax<true>},
             {"MkOpMax", parseOpMinMax<false>},
             {"MkOpToInt", parseOpToInt}},
            make_pair(AnyDomainRef(fakeIntDomain),
                      AnyExprRef(ExprRef<IntView>(nullptr))),
            essenceExpr, parsedModel);
    }
}

pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>>
parseDomainGeneratorIntFromDomain(IntDomain& domain) {
    if (domain.bounds.size() != 1) {
        cerr << "Do not currently support unrolling over int domains with "
                "holes.\n";
        abort();
    }
    auto from = make<IntValue>();
    from->value = domain.bounds.front().first;
    from->setConstant(true);

    auto to = make<IntValue>();
    to->value = domain.bounds.front().second;
    to->setConstant(true);

    auto domainExprPair =
        make_pair(fakeSequenceDomain(fakeIntDomain),
                  OpMaker<IntRange>::make(from.asExpr(), to.asExpr()));
    domainExprPair.second->setConstant(true);
    return domainExprPair;
}

pair<shared_ptr<SequenceDomain>, ExprRef<SequenceView>>
parseDomainGeneratorIntFromExpr(json& intDomainExpr, ParsedModel& parsedModel) {
    if (intDomainExpr.size() != 1) {
        cerr << "Error: do not yet support unrolling (quantifying) "
                "over int "
                "domains with holes in them.\n";
        abort();
    }
    ExprRef<IntView> from(nullptr), to(nullptr);
    auto errorFunc = [](auto&&) {
        cerr << "Expected int returning expression when parsing an int "
                "domain "
                "for unrolling.\n";
    };
    auto& rangeExpr = intDomainExpr[0];
    if (rangeExpr.count("RangeBounded")) {
        from = expect<IntView>(
            parseExpr(rangeExpr["RangeBounded"][0], parsedModel).second,
            errorFunc);
        to = expect<IntView>(
            parseExpr(rangeExpr["RangeBounded"][1], parsedModel).second,
            errorFunc);
    } else if (rangeExpr.count("RangeSingle")) {
        from = expect<IntView>(
            parseExpr(rangeExpr["RangeSingle"], parsedModel).second, errorFunc);
        to = from;
    } else {
        cerr << "Unrecognised type of int range: " << rangeExpr << endl;
        abort();
    }
    auto domainExprPair = make_pair(fakeSequenceDomain(fakeIntDomain),
                                    OpMaker<IntRange>::make(from, to));
    domainExprPair.second->setConstant(from->isConstant() && to->isConstant());
    return domainExprPair;
}

pair<AnyDomainRef, AnyExprRef> parseDomainGeneratorReference(
    json& domainExpr, ParsedModel& parsedModel) {
    auto domain = parseDomainReference(domainExpr, parsedModel);
    return mpark::visit(
        overloaded(
            [&](shared_ptr<IntDomain>& domain)
                -> pair<AnyDomainRef, AnyExprRef> {
                return parseDomainGeneratorIntFromDomain(*domain);
            },
            [&](auto& domain) -> pair<AnyDomainRef, AnyExprRef> {
                cerr << "Error: do not yet support unrolling this domain.\n";
                prettyPrint(cerr, domain) << endl;
                cerr << domainExpr << endl;
                abort();
            }),
        domain);
}

pair<AnyDomainRef, AnyExprRef> parseDomainGenerator(json& domainExpr,
                                                    ParsedModel& parsedModel) {
    auto boolGeneratorPair = stringMatch<ParseDomainGeneratorFunction>(
        {{"DomainInt", parseDomainGeneratorIntFromExpr},
         {"DomainReference", parseDomainGeneratorReference}},
        make_pair(AnyDomainRef(fakeIntDomain), ExprRef<IntView>(nullptr)),
        domainExpr, parsedModel);
    if (boolGeneratorPair.first) {
        return boolGeneratorPair.second;
    } else {
        cerr << "Error: do not yet support unrolling this domain.\n";
        cerr << domainExpr << endl;
        abort();
    }
}

void handleLettingDeclaration(json& lettingArray, ParsedModel& parsedModel) {
    string lettingName = lettingArray[0]["Name"];
    auto boolValuePair = tryParseExpr(lettingArray[1], parsedModel);
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
                cerr << "Expected Bool returning constraint within "
                        "such that: "
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
