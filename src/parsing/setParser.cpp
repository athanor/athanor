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

ParseResult parseOpSetLit(json& setExpr, ParsedModel& parsedModel) {
    auto result = parseAllAsSameType(setExpr, parsedModel);
    size_t numberElements = result.numberElements();
    auto set = OpMaker<OpSetLit>::make(move(result.exprs));
    set->setConstant(result.allConstant);
    auto domain =
        make_shared<SetDomain>(exactSize(numberElements), result.domain);
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
