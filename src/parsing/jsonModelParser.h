#ifndef SRC_PARSING_JSONMODELPARSER_H_
#define SRC_PARSING_JSONMODELPARSER_H_
#include <iostream>
#include <json.hpp>
#include <memory>
#include <unordered_map>
#include "base/base.h"
#include "types/emptyVal.h"

class ModelBuilder;

struct ParseResult {
    AnyDomainRef domain = std::make_shared<EmptyDomain>();
    AnyExprRef expr;
    bool hasEmptyType;
    ParseResult(AnyDomainRef domain, AnyExprRef expr, bool hasEmptyType)
        : domain(std::move(domain)),
          expr(std::move(expr)),
          hasEmptyType(hasEmptyType) {}
};

// in this struct, the domain matches the inner types of all exprs, either all
// have an empty type or none of them do
struct MultiParseResult {
    AnyDomainRef domain = std::make_shared<EmptyDomain>();
    AnyExprVec exprs;
    bool hasEmptyType = true;
    bool allConstant = true;
    MultiParseResult() {}
    MultiParseResult(AnyDomainRef domain, AnyExprVec exprs, bool hasEmptyType,
                     bool allConstant)
        : domain(std::move(domain)),
          exprs(std::move(exprs)),
          hasEmptyType(hasEmptyType),
          allConstant(allConstant) {}

    size_t numberElements() {
        return mpark::visit([&](auto& exprs) { return exprs.size(); }, exprs);
    }
};

struct ParsedModel {
    std::unique_ptr<ModelBuilder> builder;
    HashMap<std::string, ParseResult> namedExprs;
    HashMap<std::string, AnyDomainRef> domainLettings;
    ParsedModel();
};
ParsedModel parseModelFromJson(std::vector<nlohmann::json>& jsons);

#endif /* SRC_PARSING_JSONMODELPARSER_H_ */
