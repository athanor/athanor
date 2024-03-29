
#include "parsing/jsonModelParser.h"

#include <functional>
#include <iostream>
#include <json.hpp>
#include <unordered_map>
#include <utility>

#include "common/common.h"
#include "operators/operatorMakers.h"
#include "parsing/jsonModelParser.h"
#include "parsing/parserCommon.h"
#include "search/model.h"
#include "types/allVals.h"

using namespace std;
using namespace lib;
using namespace nlohmann;
namespace {
auto fakeBoolDomain = make_shared<BoolDomain>();
auto fakeIntDomain = make_shared<IntDomain>(vector<pair<Int, Int>>({{0, 0}}));
}  // namespace

ParsedModel::ParsedModel() : builder(make_unique<ModelBuilder>()) {}

typedef function<ParseResult(json&, ParsedModel&)> ParseDomainGeneratorFunction;
typedef function<ParseResult(json&, ParsedModel&)> ParseExprFunction;
typedef function<AnyDomainRef(json&, ParsedModel&)> ParseDomainFunction;

ParseResult parseOpSetLit(json& setExpr, ParsedModel& parsedModel);
ParseResult parseOpPartitionLit(json& partitionExpr, ParsedModel& parsedModel);
ParseResult parseOpFunctionRange(json& functionExpr, ParsedModel& parsedModel);
ParseResult parseOpToSet(json& sequenceExpr, ParsedModel& parsedModel);
ParseResult parseOpFunctionDefined(json& functionExpr,
                                   ParsedModel& parsedModel);
ParseResult parseOpFunctionPreimage(json& preimageOpExpr,
                                    ParsedModel& parsedModel);
ParseResult parseOpMatrixLit(json& matrixExpr, ParsedModel& parsedModel);
ParseResult parseOpFunctionLit(json& functionExpr, ParsedModel& parsedModel);
ParseResult parseOpRecordLit(json& tupleExpr, ParsedModel& parsedModel);
ParseResult parseOpTupleLit(json& tupleExpr, ParsedModel& parsedModel);
ParseResult parseConstantInt(json& intExpr, ParsedModel&);
ParseResult parseConstantBool(json& boolExpr, ParsedModel&);
ParseResult parseOpTwoBars(json& operandExpr, ParsedModel& parsedModel);
shared_ptr<EnumDomain> parseDomainEnum(json& enumDomainExpr,
                                       ParsedModel& parsedModel);
shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel);
void handleEnumDeclaration(json& enumDeclExpr, ParsedModel& parsedModel);
void handleUnnamedTypeDeclaration(json& enumDeclExpr, ParsedModel& parsedModel);
shared_ptr<BoolDomain> parseDomainBool(json&, ParsedModel&);
shared_ptr<SetDomain> parseDomainSet(json& setDomainExpr,
                                     ParsedModel& parsedModel);
shared_ptr<SetDomain> parseDomainRelation(json& relationDomainExpr,
                                          ParsedModel& parsedModel);
shared_ptr<MSetDomain> parseDomainMSet(json& mSetDomainExpr,
                                       ParsedModel& parsedModel);
shared_ptr<SequenceDomain> parseDomainSequence(json& sequenceDomainExpr,
                                               ParsedModel& parsedModel);
shared_ptr<SequenceDomain> parseDomainSequence(json& sequenceDomainExpr,
                                               ParsedModel& parsedModel);
shared_ptr<FunctionDomain> parseDomainFunction(json& functionDomainExpr,
                                               ParsedModel& parsedModel);
shared_ptr<FunctionDomain> parseDomainMatrix(json& matrixDomainExpr,
                                             ParsedModel& parsedModel);
shared_ptr<TupleDomain> parseDomainTuple(json& tupleDomainExpr,
                                         ParsedModel& parsedModel);
shared_ptr<TupleDomain> parseDomainRecord(json& tupleDomainExpr,
                                          ParsedModel& parsedModel);
shared_ptr<PartitionDomain> parseDomainPartition(json& partitionDomainExpr,
                                                 ParsedModel& parsedModel);
ParseResult parseConstantInt(json& intExpr, ParsedModel&);
ParseResult parseOpMod(json& modExpr, ParsedModel& parsedModel);
ParseResult parseOpDiv(json& modExpr, ParsedModel& parsedModel);
ParseResult parseOpMinus(json& minusExpr, ParsedModel& parsedModel);
ParseResult parseOpPower(json& powerExpr, ParsedModel& parsedModel);
ParseResult parseOpNegate(json& modExpr, ParsedModel& parsedModel);
ParseResult parseOpSequenceLit(json&, ParsedModel&);
ParseResult parseOpMSetLit(json&, ParsedModel&);
ParseResult parseOpIn(json& inExpr, ParsedModel& parsedModel);
ParseResult parseOpNot(json&, ParsedModel&);
ParseResult bsetEq(json& subsetExpr, ParsedModel& parsedModel);
ParseResult parseOpSubset(json& operandsExpr, ParsedModel& parsedModel);
ParseResult parseOpSubsetEq(json& operandsExpr, ParsedModel& parsedModel);
ParseResult parseOpIntersect(json& intersectExpr, ParsedModel& parsedModel);
ParseResult parseOpAllDiff(json& operandExpr, ParsedModel& parsedModel);
ParseResult parseOpPartitionParty(json& partsExpr, ParsedModel& parsedModel);
ParseResult parseOpPartitionParts(json& partsExpr, ParsedModel& parsedModel);
ParseResult parseOpPowerSet(json& powerSetExpr, ParsedModel& parsedModel);
ParseResult parseOpSequenceIndex(AnyDomainRef& innerDomain,
                                 ExprRef<SequenceView>& sequence,
                                 json& indexExpr, bool hasEmptyType,
                                 ParsedModel& parsedModel);
ParseResult parseOpFunctionImage(const FunctionDomain& domain,
                                 ExprRef<FunctionView>& function,
                                 json& preImageExpr, bool hasEmptyType,
                                 ParsedModel& parsedModel);
ParseResult parseOpTupleIndex(shared_ptr<TupleDomain>& tupleDomain,
                              ExprRef<TupleView>& tuple, json& indexExpr,
                              ParsedModel& parsedModel);
ParseResult parseOpRecordIndex(shared_ptr<TupleDomain>& tupleDomain,
                               ExprRef<TupleView>& tuple, json& indexExpr,
                               ParsedModel&);
ParseResult parseOpEq(json& operandsExpr, ParsedModel& parsedModel);
ParseResult parseOpNotEq(json& operandsExpr, ParsedModel& parsedModel);
ParseResult parseOpTogether(json& operandsExpr, ParsedModel& parsedModel);
ParseResult parseOpApart(json& operandsExpr, ParsedModel& parsedModel);
template <bool>
ParseResult parseOpLess(json& expr, ParsedModel& parsedModel);
template <bool>
ParseResult parseOpLessEq(json& expr, ParsedModel& parsedModel);
ParseResult parseOpImplies(json& expr, ParsedModel& parsedModel);
ParseResult parseComprehension(json& comprExpr, ParsedModel& parsedModel);
ParseResult parseOpToInt(json& expr, ParsedModel& parsedModel);
// fudge

ParseResult parseExpr(json& essenceExpr, ParsedModel& parsedModel) {
    auto constraint = tryParseExpr(essenceExpr, parsedModel);
    if (constraint) {
        return move(*constraint);
    } else {
        myCerr << "Failed to parse expression: " << essenceExpr << endl;
        myAbort();
    }
}

AnyDomainRef parseDomain(json& essenceExpr, ParsedModel& parsedModel) {
    auto domain = tryParseDomain(essenceExpr, parsedModel);
    if (domain) {
        return move(*domain);
    } else {
        myCerr << "Failed to parse domain: " << essenceExpr << endl;
        myAbort();
    }
}

ParseResult parseValueReference(json& essenceReference,
                                ParsedModel& parsedModel) {
    string referenceName = essenceReference[0]["Name"];
    if (parsedModel.namedExprs.count(referenceName)) {
        return parsedModel.namedExprs.at(referenceName);
    } else {
        myCerr << "Found reference to value with name \"" << referenceName
               << "\" but this does not appear to be in "
                  "scope.\n";
        myExit(1);
    }
}

AnyDomainRef parseDomainReference(json& domainReference,
                                  ParsedModel& parsedModel) {
    string referenceName = domainReference[0]["Name"];
    if (!parsedModel.domainLettings.count(referenceName)) {
        myCerr << "Found reference to domainwith name \"" << referenceName
               << "\" but this does not appear to be in "
                  "scope.\n"
               << domainReference << endl;
        myAbort();
    } else {
        return parsedModel.domainLettings.at(referenceName);
    }
}

optional<AnyDomainRef> tryParseDomain(json& domainExpr,
                                      ParsedModel& parsedModel) {
    return stringMatch<ParseDomainFunction>(
        {{"DomainInt", parseDomainInt},
         {"DomainEnum", parseDomainEnum},
         {"DomainBool", parseDomainBool},
         {"DomainSet", parseDomainSet},
         {"DomainRelation", parseDomainRelation},
         {"DomainMSet", parseDomainMSet},
         {"DomainSequence", parseDomainSequence},
         {"DomainMatrix", parseDomainMatrix},
         {"DomainTuple", parseDomainTuple},
         {"DomainRecord", parseDomainRecord},
         {"DomainFunction", parseDomainFunction},
         {"DomainPartition", parseDomainPartition},
         {"DomainReference", parseDomainReference}},
        domainExpr, parsedModel);
}

optional<ParseResult> tryParseSpecialConstraint(json& operandsExpr,
                                                ParsedModel& parsedModel) {
    if (operandsExpr[0].count("Reference") &&
        operandsExpr[0]["Reference"][0]["Name"] == "amplify") {
        cout
            << "[warning] treating \"amplify\" as a special athanor keyword.\n";
        auto& args = operandsExpr[1][0]["AbstractLiteral"]["AbsLitTuple"];
        auto boolConstraint = expect<BoolView>(
            parseExpr(args[0], parsedModel).expr,
            [&](auto&) { cerr << "Whilst parsing amplify.\n"; });
        Int multiplier =
            parseExprAsInt(args[1], parsedModel, "whilst parsing amplify.");
        return ParseResult(
            fakeBoolDomain,
            OpMaker<OpAmplifyConstraint>::make(boolConstraint, multiplier),
            false);
    }
    return nullopt;
}
ParseResult parseOpRelationProj(json& operandsExpr, ParsedModel& parsedModel) {
    optional<ParseResult> parseSpecialConstraint =
        tryParseSpecialConstraint(operandsExpr, parsedModel);
    if (parseSpecialConstraint) {
        return *parseSpecialConstraint;
    }
    auto leftOperand = parseExpr(operandsExpr[0], parsedModel);
    return lib::visit(
        overloaded(
            [&](ExprRef<SequenceView>& sequence) -> ParseResult {
                auto& innerDomain =
                    lib::get<shared_ptr<SequenceDomain>>(leftOperand.domain)
                        ->inner;
                return parseOpSequenceIndex(
                    innerDomain, sequence, operandsExpr[1][0],
                    leftOperand.hasEmptyType, parsedModel);
            },
            [&](ExprRef<FunctionView>& function) -> ParseResult {
                auto& functionDomain =
                    lib::get<shared_ptr<FunctionDomain>>(leftOperand.domain);
                return parseOpFunctionImage(
                    *functionDomain, function, operandsExpr[1][0],
                    leftOperand.hasEmptyType, parsedModel);
            },
            [&](auto&& operand) -> ParseResult {
                myCerr << "Error, not yet handling op "
                          "relation projection "
                          "with a "
                          "left operand "
                          "of type "
                       << TypeAsString<typename AssociatedValueType<viewType(
                              operand)>::type>::value
                       << ": " << operandsExpr << endl;
                myAbort();
            }),
        leftOperand.expr);
}

ParseResult parseOpIndexingHelper(json& operandsExpr, ParsedModel& parsedModel,
                                  ParseResult& leftOperand,
                                  size_t currentIndex) {
    auto indexedItem = lib::visit(
        overloaded(
            [&](ExprRef<TupleView>& tuple) -> ParseResult {
                auto& tupleDomain =
                    lib::get<shared_ptr<TupleDomain>>(leftOperand.domain);
                if (tupleDomain->isRecord) {
                    return parseOpRecordIndex(tupleDomain, tuple,
                                              operandsExpr[currentIndex],
                                              parsedModel);
                } else {
                    return parseOpTupleIndex(tupleDomain, tuple,
                                             operandsExpr[currentIndex],
                                             parsedModel);
                }
            },
            [&](ExprRef<SequenceView>& sequence) -> ParseResult {
                auto& sequenceDomain =
                    lib::get<shared_ptr<SequenceDomain>>(leftOperand.domain);
                return parseOpSequenceIndex(
                    sequenceDomain->inner, sequence, operandsExpr[currentIndex],
                    leftOperand.hasEmptyType, parsedModel);
            },
            [&](ExprRef<FunctionView>& function) -> ParseResult {
                auto& functionDomain =
                    lib::get<shared_ptr<FunctionDomain>>(leftOperand.domain);
                return parseOpFunctionImage(
                    *functionDomain, function, operandsExpr[currentIndex],
                    leftOperand.hasEmptyType, parsedModel);
            },
            [&](auto&& operand) -> ParseResult {
                myCerr << "Error, not yet handling op "
                          "Indexing "
                          "with a "
                          "left operand "
                          "of type "
                       << TypeAsString<typename AssociatedValueType<viewType(
                              operand)>::type>::value
                       << ": " << operandsExpr << endl;
                myAbort();
            }),
        leftOperand.expr);
    if (currentIndex + 1 == operandsExpr.size()) {
        return indexedItem;
    } else {
        return parseOpIndexingHelper(operandsExpr, parsedModel, indexedItem,
                                     currentIndex + 1);
    }
}

ParseResult parseOpIndexing(json& operandsExpr, ParsedModel& parsedModel) {
    auto leftOperand = parseExpr(operandsExpr[0], parsedModel);
    return parseOpIndexingHelper(operandsExpr, parsedModel, leftOperand, 1);
}

ParseResult parseOpCatchUndef(json& operandsExpr, ParsedModel& parsedModel) {
    auto left = parseExpr(operandsExpr[0], parsedModel);
    auto right = parseExpr(operandsExpr[1], parsedModel);
    bool leftHadEmptyType = left.hasEmptyType;
    bool rightHadEmptyType = right.hasEmptyType;
    // catch undef can switch  between the left and right views.
    // make sure their domains are merged
    mergeDomains(left.domain, right.domain);
    left.hasEmptyType = hasEmptyDomain(left.domain);
    right.domain = left.domain;
    right.hasEmptyType = left.hasEmptyType;
    if (leftHadEmptyType) {
        tryRemoveEmptyType(left.domain, left.expr);
    }
    if (rightHadEmptyType) {
        tryRemoveEmptyType(right.domain, right.expr);
    }

    return lib::visit(
        [&](auto& leftExpr) {
            typedef viewType(leftExpr) View;
            typedef typename AssociatedValueType<View>::type Value;
            // this error handler will prob not be called as the merging of
            // domains above will throw an error if the types do not match.
            auto errorHandler = [&](auto&&) {
                myCerr << "Expected right operand to be of "
                          "same type as left, "
                          "i.e. "
                       << TypeAsString<Value>::value << endl;
            };
            auto rightExpr = expect<View>(right.expr, errorHandler);
            auto op = OpMaker<OpCatchUndef<View>>::make(leftExpr, rightExpr);
            op->setConstant(leftExpr->isConstant() && rightExpr->isConstant());
            return ParseResult(left.domain, op, left.hasEmptyType);
        },
        left.expr);
}

template <typename Op, typename SequenceInnerViewType>
ParseResult parseSequenceFoldingOp(json& operandExpr,
                                   ParsedModel& parsedModel) {
    static_assert(is_same<BoolView, SequenceInnerViewType>::value ||
                      is_same<IntView, SequenceInnerViewType>::value,
                  "This function currently supports SequenceInnerViewTypes of "
                  "int or bool only.\n");
    typedef typename AssociatedDomain<SequenceInnerViewType>::type
        ExpectedInnerDomain;
    typedef typename AssociatedValueType<SequenceInnerViewType>::type
        ExpectedInnerValue;
    ParseResult parsedOperandExpr =
        toSequence(parseExpr(operandExpr, parsedModel));
    auto& sequenceDomain =
        lib::get<shared_ptr<SequenceDomain>>(parsedOperandExpr.domain);
    auto& sequenceOperand =
        lib::get<ExprRef<SequenceView>>(parsedOperandExpr.expr);
    auto op = OpMaker<Op>::make(sequenceOperand, sequenceDomain);
    if (!lib::get_if<shared_ptr<ExpectedInnerDomain>>(
            &(sequenceDomain->inner)) &&
        !lib::get_if<shared_ptr<EmptyDomain>>(&(sequenceDomain->inner))) {
        myCerr << "Error: expected sequence with inner type "
               << TypeAsString<ExpectedInnerValue>::value << " for op "
               << op->getOpName() << endl;
        myCerr << *sequenceDomain << endl;
        myExit(1);
    }
    auto domain = (is_same<BoolView, SequenceInnerViewType>::value)
                      ? AnyDomainRef(fakeBoolDomain)
                      : AnyDomainRef(fakeIntDomain);
    return ParseResult(domain, op, false);
}

optional<ParseResult> tryParseExpr(json& essenceExpr,
                                   ParsedModel& parsedModel) {
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
             {"AbsLitPartition", parseOpPartitionLit},
             {"AbsLitSequence", parseOpSequenceLit},
             {"AbsLitMatrix", parseOpMatrixLit},
             {"AbsLitMSet", parseOpMSetLit},
             {"AbsLitFunction", parseOpFunctionLit},
             {"AbsLitTuple", parseOpTupleLit},
             {"AbsLitRecord", parseOpRecordLit},
             {"Reference", parseValueReference},
             {"Comprehension", parseComprehension},
             {"MkOpEq", parseOpEq},
             {"MkOpIff", parseOpEq},
             {"MkOpLt", parseOpLess<false>},
             {"MkOpLeq", parseOpLessEq<false>},
             {"MkOpGt", parseOpLess<true>},
             {"MkOpGeq", parseOpLessEq<true>},
             {"MkOpNeq", parseOpNotEq},
             {"MkOpImply", parseOpImplies},
             {"MkOpCatchUndef", parseOpCatchUndef},
             {"MkOpIn", parseOpIn},
             {"MkOpMod", parseOpMod},
             {"MkOpNegate", parseOpNegate},
             {"MkOpNot", parseOpNot},
             {"MkOpDiv", parseOpDiv},
             {"MkOpPreImage", parseOpFunctionPreimage},
             {"MkOpImage", parseOpIndexing},
             {"MkOpRange", parseOpFunctionRange},
             {"MkOpToSet", parseOpToSet},
             {"MkOpDefined", parseOpFunctionDefined},
             {"MkOpMinus", parseOpMinus},
             {"MkOpPow", parseOpPower},
             {"MkOpIntersect", parseOpIntersect},
             {"MkOpTogether", parseOpTogether},
             {"MkOpApart", parseOpApart},
             {"MkOpTwoBars", parseOpTwoBars},
             {"MkOpPowerSet", parseOpPowerSet},
             {"MkOpSubsetEq", parseOpSubsetEq},
             {"MkOpSubset", parseOpSubset},
             {"MkOpAnd", parseSequenceFoldingOp<OpAnd, BoolView>},
             {"MkOpOr", parseSequenceFoldingOp<OpOr, BoolView>},
             {"MkOpSum", parseSequenceFoldingOp<OpSum, IntView>},
             {"MkOpProduct", parseSequenceFoldingOp<OpProd, IntView>},
             {"MkOpMin", parseSequenceFoldingOp<OpMin, IntView>},
             {"MkOpMax", parseSequenceFoldingOp<OpMax, IntView>},
             {"MkOpAllDiff", parseOpAllDiff},
             {"MkOpRelationProj", parseOpRelationProj},
             {"MkOpIndexing", parseOpIndexing},
             {"MkOpParts", parseOpPartitionParts},
             {"MkOpParty", parseOpPartitionParty},
             {"MkOpToInt", parseOpToInt}},
            essenceExpr, parsedModel);
    }
}

void handleLettingDeclaration(json& lettingArray, ParsedModel& parsedModel) {
    string lettingName = lettingArray[0]["Name"];
    debug_log("Parsing letting " << lettingName);
    auto value = tryParseExpr(lettingArray[1], parsedModel);
    if (value) {
        parsedModel.namedExprs.emplace(lettingName, *value);
        return;
    }
    if (lettingArray[1].count("Domain")) {
        auto domain = parseDomain(lettingArray[1]["Domain"], parsedModel);
        parsedModel.domainLettings.emplace(lettingName, domain);
        return;
    }
    myCerr << "Not sure how to parse this letting: " << lettingArray << endl;
    myAbort();
}

void handleFindDeclaration(json& findArray, ParsedModel& parsedModel) {
    string findName = findArray[1]["Name"];
    debug_log("Parsing find " << findName);
    auto findDomain = parseDomain(findArray[2], parsedModel);
    lib::visit(
        [&](auto& domainImpl) {
            parsedModel.namedExprs.emplace(
                findName, ParseResult(domainImpl,
                                      parsedModel.builder
                                          ->addVariable(findName, domainImpl)
                                          .asExpr(),
                                      false));
        },
        findDomain);
}

void parseExprs(json& suchThat, ParsedModel& parsedModel) {
    debug_log("Parsing such that");
    for (auto& op : suchThat) {
        debug_log("parsing op");
        ExprRef<BoolView> constraint =
            expect<BoolView>(parseExpr(op, parsedModel).expr, [&](auto&&) {
                myCerr << "Expected Bool returning constraint "
                          "within "
                          "such that: "
                       << op << endl;
            });
        parsedModel.builder->addConstraint(constraint);
    }
}

void validateObjectiveType(json& expr, AnyDomainRef domain) {
    const char* errorMessage =
        "Error: objective must be int or tuple of int.\n";
    lib::visit(
        overloaded([&](const shared_ptr<IntDomain>&) {},
                   [&](const shared_ptr<TupleDomain>& domain) {
                       for (auto& inner : domain->inners) {
                           if (!lib::get_if<shared_ptr<IntDomain>>(&inner)) {
                               myCerr << errorMessage;
                               myCerr << expr << endl;
                               myAbort();
                           }
                       }
                   },
                   [&](const auto&) {
                       myCerr << errorMessage;
                       myCerr << expr << endl;
                       myAbort();
                   }),
        domain);
}

inline void parseObjective(json& objExpr, ParsedModel& parsedModel) {
    string modeStr = objExpr[0];
    OptimiseMode mode = (modeStr == "Minimising") ? OptimiseMode::MINIMISE
                                                  : OptimiseMode::MAXIMISE;
    auto objective = parseExpr(objExpr[1], parsedModel);
    validateObjectiveType(objExpr[1], objective.domain);
    parsedModel.builder->setObjective(mode, objective.expr);
}

void parseJson(json& j, ParsedModel& parsedModel) {
    for (auto& statement : j["mStatements"]) {
        if (statement.count("Declaration")) {
            auto& declaration = statement["Declaration"];
            if (declaration.count("Letting")) {
                handleLettingDeclaration(declaration["Letting"], parsedModel);
            } else if (declaration.count("FindOrGiven") &&
                       declaration["FindOrGiven"][0] == "Find") {
                handleFindDeclaration(declaration["FindOrGiven"], parsedModel);
            } else if (declaration.count("LettingDomainDefnEnum")) {
                handleEnumDeclaration(declaration["LettingDomainDefnEnum"],
                                      parsedModel);
            } else if (declaration.count("LettingDomainDefnUnnamed")) {
                handleUnnamedTypeDeclaration(
                    declaration["LettingDomainDefnUnnamed"], parsedModel);
            }
        } else if (statement.count("SuchThat")) {
            parseExprs(statement["SuchThat"], parsedModel);
        } else if (statement.count("Objective")) {
            parseObjective(statement["Objective"], parsedModel);
        }
    }
}

ParsedModel parseModelFromJson(vector<json>& jsons) {
    ParsedModel parsedModel;
    for (auto& json : jsons) {
        parseJson(json, parsedModel);
    }
    return parsedModel;
}
