#ifndef SRC_JSONMODELPARSER_H_
#define SRC_JSONMODELPARSER_H_
#include <iostream>
#include <memory>
#include <unordered_map>
#include "operators/operatorBase.h"
#include "base/base.h"

class ModelBuilder;
struct ParsedModel {
    std::unique_ptr<ModelBuilder> builder;
    std::unordered_map<std::string, std::pair<AnyDomainRef, AnyExprRef>>
        namedExprs;
    std::unordered_map<std::string, AnyDomainRef> domainLettings;
    ParsedModel();
};

ParsedModel parseModelFromJson(std::istream& is);

#endif /* SRC_JSONMODELPARSER_H_ */
