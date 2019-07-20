#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/mSetVal.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
shared_ptr<MSetDomain> parseDomainMSet(json& mSetDomainExpr,
                                       ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(mSetDomainExpr[1][0], parsedModel);
    if (!mSetDomainExpr[1][1].count("OccurAttr_None")) {
        cerr << "Error: for the moment, given attribute "
                "must be "
                "OccurAttr_None.  This is not handled yet: "
             << mSetDomainExpr[1][1] << endl;
        abort();
    }
    return make_shared<MSetDomain>(sizeAttr,
                                   parseDomain(mSetDomainExpr[2], parsedModel));
}

ParseResult parseOpMSetLit(json& mSetExpr, ParsedModel& parsedModel) {
    auto result = parseAllAsSameType(mSetExpr, parsedModel);
    size_t numberElements = result.numberElements();
    auto mSet = OpMaker<OpMSetLit>::make(move(result.exprs));
    mSet->setConstant(result.allConstant);
    auto domain =
        make_shared<MSetDomain>(exactSize(numberElements), result.domain);
    return ParseResult(domain, mSet, result.hasEmptyType);
}
