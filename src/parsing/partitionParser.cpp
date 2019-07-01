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
    SizeAttr sizeAttr =
        parseSizeAttr(partitionDomainExpr[1]["partsNum"], parsedModel);
    if (!regular || sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE) {
        cerr << "Only supporting partitions that are "
                "regular and a fixed "
                "number of parts.  Make sure these "
                "attributes are specified.\n";
        abort();
    }
    return make_shared<PartitionDomain>(
        parseDomain(partitionDomainExpr[2], parsedModel), regular,
        sizeAttr.minSize);
}
