#ifndef SRC_JSONMODELPARSER_H_
#define SRC_JSONMODELPARSER_H_
#include <iostream>
#include <memory>
#include <unordered_map>
#include "operators/operatorBase.h"
#include "types/base.h"

class ModelBuilder;
struct ParsedModel {
    std::unique_ptr<ModelBuilder> builder;
    std::unordered_map<std::string, std::pair<AnyDomainRef, AnyValRef>> vars;
    std::unordered_map<std::string, std::pair<AnyDomainRef, AnyIterRef>>
        scopedVars;
    std::unordered_map<std::string, AnyDomainRef> domainLettings;
    std::unordered_map<std::string, AnyExprRef> constantExprs;
    ParsedModel();
};

ParsedModel parseModelFromJson(std::istream& is);

#endif /* SRC_JSONMODELPARSER_H_ */
