#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/boolVal.h"
#include "types/partitionVal.h"
#include "types/setVal.h"
#include "types/sizeAttr.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
namespace {

auto fakeBoolDomain = make_shared<BoolDomain>();
}

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

ParseResult parseOpPartitionParty(json& partsExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected partition returning expression within Op "
        "PartitionParty: ";
    auto partyMemberExpr = parseExpr(partsExpr[0], parsedModel);
    auto partitionExpr = parseExpr(partsExpr[1], parsedModel);
    auto operand = expect<PartitionView>(partitionExpr.expr, [&](auto&&) {
        myCerr << errorMessage << partsExpr << endl;
    });
    return lib::visit(
        [&](auto& innerDomain) {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<InnerDomainType>::type
                InnerViewType;
            auto partyMember =
                expect<InnerViewType>(partyMemberExpr.expr, [&](auto&) {
                    myCerr << "Expected party member to be same type as "
                              "partition member.\n";
                });
            bool constant = partyMember->isConstant() && operand->isConstant();
            auto op = OpMaker<OpPartitionParty<viewType(partyMember)>>::make(
                partyMember, operand);
            op->setConstant(constant);
            return ParseResult(
                make_shared<SetDomain>(noSize(), partyMemberExpr.domain), op,
                partitionExpr.hasEmptyType);
        },
        lib::get<shared_ptr<PartitionDomain>>(partitionExpr.domain)->inner);
}

ParseResult parseOpTogether(json& operandsExpr, ParsedModel& parsedModel) {
    auto set = expect<SetView>(
        parseExpr(operandsExpr[0], parsedModel).expr, [&](auto&&) {
            myCerr << "Error, together takes a set as its "
                      "first parameter.\n";
        });

    auto partition = expect<PartitionView>(
        parseExpr(operandsExpr[1], parsedModel).expr, [&](auto&&) {
            myCerr << "Error, together takes a partition as "
                      "its second "
                      "parameter.\n";
        });
    auto op = OpMaker<OpTogether>::make(set, partition);
    op->setConstant(set->isConstant() && partition->isConstant());
    return ParseResult(fakeBoolDomain, op, false);
}

ParseResult parseOpApart(json& operandsExpr, ParsedModel& parsedModel) {
    auto result = parseOpTogether(operandsExpr, parsedModel);
    auto& expr = lib::get<ExprRef<BoolView>>(result.expr);
    auto negated = OpMaker<OpNot>::make(expr);
    negated->setConstant(expr->isConstant());
    return ParseResult(result.domain, negated, result.hasEmptyType);
}
