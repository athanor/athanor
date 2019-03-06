#ifndef SRC_JSONMODELPARSER_H_
#define SRC_JSONMODELPARSER_H_
#include <iostream>
#include <memory>
#include <unordered_map>
#include "base/base.h"

class ModelBuilder;
struct ParsedModel {
    std::unique_ptr<ModelBuilder> builder;
    HashMap<std::string, std::pair<AnyDomainRef, AnyExprRef>>
        namedExprs;
    HashMap<std::string, AnyDomainRef> domainLettings;
    ParsedModel();
};
typedef std::vector<std::reference_wrapper<std::istream>> ISRefVec;
ParsedModel parseModelFromJson(ISRefVec v);

#endif /* SRC_JSONMODELPARSER_H_ */
