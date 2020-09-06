#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/partitionVal.h"
#include "types/setVal.h"
#include "types/sizeAttr.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
shared_ptr<PartitionDomain> parseDomainPartition(json& partitionDomainExpr,
                                                 ParsedModel& parsedModel) {
    bool regular = partitionDomainExpr[1]["isRegular"];
    SizeAttr numberParts =
        parseSizeAttr(partitionDomainExpr[1]["partsNum"], parsedModel);
    SizeAttr partSize =
        parseSizeAttr(partitionDomainExpr[1]["partsSize"], parsedModel);
    auto domain = make_shared<PartitionDomain>(
        parseDomain(partitionDomainExpr[2], parsedModel), regular, numberParts,
        partSize);
    //    prettyPrint(cout, *domain);
    return domain;
}

ParseResult parseOpPartitionParts(json& partsExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected partition returning expression within Op "
        "PartitionParts: ";
    auto parsedExpr = parseExpr(partsExpr, parsedModel);
    auto operand = expect<PartitionView>(parsedExpr.expr, [&](auto&&) {
        myCerr << errorMessage << partsExpr << endl;
    });
    auto& domain = lib::get<shared_ptr<PartitionDomain>>(parsedExpr.domain);
    bool constant = operand->isConstant();
    auto op = OpMaker<OpPartitionParts>::make(operand);
    op->setConstant(constant);
    return ParseResult(
        make_shared<SetDomain>(
            domain->numberParts,
            make_shared<SetDomain>(domain->partSize, domain->inner)),
        op, parsedExpr.hasEmptyType);
}
