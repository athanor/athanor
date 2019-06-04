#ifndef SRC_JSONMODELPARSER_H_
#define SRC_JSONMODELPARSER_H_
#include <iostream>
#include <json.hpp>
#include <memory>
#include <unordered_map>
#include "base/base.h"

class ModelBuilder;
struct ParsedModel {
    std::unique_ptr<ModelBuilder> builder;
    HashMap<std::string, std::pair<AnyDomainRef, AnyExprRef>> namedExprs;
    HashMap<std::string, AnyDomainRef> domainLettings;
    ParsedModel();
};
ParsedModel parseModelFromJson(std::vector<nlohmann::json>& jsons);

#endif /* SRC_JSONMODELPARSER_H_ */
