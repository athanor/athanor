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
    auto& domain = mpark::get<shared_ptr<SetDomain>>(parsedExpr.domain);
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
