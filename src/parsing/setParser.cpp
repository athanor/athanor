#include <algorithm>

#include "parsing/parserCommon.h"
#include "types/allVals.h"
using namespace std;
using namespace lib;
using namespace nlohmann;

shared_ptr<SetDomain> parseDomainSet(json& setDomainExpr,
                                     ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(setDomainExpr[1], parsedModel);
    return make_shared<SetDomain>(sizeAttr,
                                  parseDomain(setDomainExpr[2], parsedModel));
}

shared_ptr<TupleDomain> parseDomainTuple(json& tupleDomainExpr,
                                         ParsedModel& parsedModel);
shared_ptr<SetDomain> parseDomainRelation(json& relationDomainExpr,
                                          ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(relationDomainExpr[1][0], parsedModel);
    auto domain = make_shared<SetDomain>(
        sizeAttr, parseDomainTuple(relationDomainExpr[2], parsedModel));
    domain->isRelation = true;
    return domain;
}

ParseResult parseOpPowerSet(json& powerSetExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected set returning expression within Op "
        "PowerSet: ";
    auto parsedExpr = parseExpr(powerSetExpr, parsedModel);
    auto operand = expect<SetView>(parsedExpr.expr, [&](auto&&) {
        myCerr << errorMessage << powerSetExpr << endl;
    });
    auto& domain = lib::get<shared_ptr<SetDomain>>(parsedExpr.domain);
    bool constant = operand->isConstant();
    auto op = OpMaker<OpPowerSet>::make(move(operand));
    op->setConstant(constant);
    return ParseResult(
        make_shared<SetDomain>(noSize(),
                               make_shared<SetDomain>(noSize(), domain->inner)),
        op, parsedExpr.hasEmptyType);
}

ExprRef<FunctionView> makeBasicFunctionLitFrom(MultiParseResult result) {
    size_t numberElements = result.numberElements();
    return lib::visit(
        [&](auto& domain) {
            typedef typename BaseType<decltype(domain)>::element_type Domain;
            typedef typename AssociatedViewType<Domain>::type View;
            auto preImageDomain = make_shared<IntDomain>(
                vector<pair<Int, Int>>({{0, numberElements - 1}}));
            auto function =
                OpMaker<OpFunctionLitBasic>::make<View>(preImageDomain);
            auto& functionView = *function->view();
            functionView.initView(preImageDomain,
                                  makeDimensionVecFromDomain(preImageDomain),
                                  move(result.exprs), false);
            function->setConstant(result.allConstant);
            return function;
        },
        result.domain);
}

ParseResult parseOpSetLit(json& setExpr, ParsedModel& parsedModel) {
    auto result = parseAllAsSameType(setExpr, parsedModel);
    size_t numberElements = result.numberElements();
    bool allConstant = result.allConstant;
    auto innerDomain = result.domain;
    // OpSetLit is designed to build a set from another container of members,
    // such as function or sequence.  Here, we put members in a function lit and
    // give as operand to OpSetLit.

    auto function = makeBasicFunctionLitFrom(move(result));
    auto set = OpMaker<OpSetLit<FunctionView>>::make(function);
    set->setConstant(allConstant);
    auto domain = make_shared<SetDomain>(maxSize(numberElements), innerDomain);
    return ParseResult(domain, set, result.hasEmptyType);
}

ParseResult parseOpIntersect(json& intersectExpr, ParsedModel& parsedModel) {
    auto left = parseExpr(intersectExpr[0], parsedModel);
    auto right = parseExpr(intersectExpr[1], parsedModel);
    return mpark::visit(
        overloaded(
            [&](shared_ptr<SetDomain>& leftDomain,
                shared_ptr<SetDomain>& rightDomain) -> ParseResult {
                auto& returnDomain =
                    (left.hasEmptyType) ? leftDomain : rightDomain;
                auto& leftSet = mpark::get<ExprRef<SetView>>(left.expr);
                auto& rightSet = mpark::get<ExprRef<SetView>>(right.expr);
                auto op = OpMaker<OpSetIntersect>::make(leftSet, rightSet);
                op->setConstant(leftSet->isConstant() &&
                                rightSet->isConstant());
                return ParseResult(returnDomain, op,
                                   !left.hasEmptyType || !right.hasEmptyType);
            },
            [&](auto&, auto&) -> ParseResult {
                myCerr << "only supporting intersect for set.\n";
                myCerr << intersectExpr;
                myAbort();
            }),
        left.domain, right.domain);
}

ParseResult parseOpFunctionRange(json& functionExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected function returning expression within Op "
        "FunctionRange: ";
    auto parsedExpr = parseExpr(functionExpr, parsedModel);
    auto operand = expect<FunctionView>(parsedExpr.expr, [&](auto&&) {
        myCerr << errorMessage << functionExpr << endl;
    });
    auto& domain = lib::get<shared_ptr<FunctionDomain>>(parsedExpr.domain)->to;
    bool constant = operand->isConstant();
    auto op = OpMaker<OpSetLit<FunctionView>>::make(move(operand));
    op->setConstant(constant);
    return ParseResult(make_shared<SetDomain>(noSize(), domain), op,
                       parsedExpr.hasEmptyType);
}

ParseResult parseOpFunctionPreimage(json& preimageOpExpr,
                                    ParsedModel& parsedModel) {
    auto functionParseResult = parseExpr(preimageOpExpr[0], parsedModel);
    auto function = expect<FunctionView>(functionParseResult.expr, [&](auto&) {
        myCerr
            << "Expected function expr as first argument to function preimage\n"
            << preimageOpExpr << endl;
    });
    auto imageResult = parseExpr(preimageOpExpr[1], parsedModel);
    auto preimageDomain =
        lib::get<shared_ptr<FunctionDomain>>(functionParseResult.domain)->from;
    return lib::visit(
        [&](auto& imageDomain) {
            typedef typename BaseType<decltype(imageDomain)>::element_type
                ImageDomain;
            typedef typename AssociatedViewType<ImageDomain>::type ImageView;
            auto image = lib::get<ExprRef<ImageView>>(imageResult.expr);
            auto op =
                OpMaker<OpFunctionPreimage<ImageView>>::make(image, function);
            op->setConstant(function->isConstant() && image->isConstant());
            return ParseResult(make_shared<SetDomain>(noSize(), preimageDomain),
                               op, functionParseResult.hasEmptyType);
        },
        imageResult.domain);
}
