#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/partitionVal.h"
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
