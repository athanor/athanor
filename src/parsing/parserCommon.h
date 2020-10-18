#ifndef SRC_PARSING_PARSERCOMMON_H_
#define SRC_PARSING_PARSERCOMMON_H_
#include <functional>
#include <iostream>
#include <json.hpp>
#include <memory>
#include <unordered_map>
#include "base/base.h"
#include "operators/operatorMakers.h"
#include "parsing/jsonModelParser.h"
// refactorring the json model parser into several files.  Just started, this
// will be a long process.  This is the beginnings of a shared common header
// which will contain the necessary prototypes of course.

bool hasEmptyDomain(const AnyDomainRef& domain);
void tryRemoveEmptyType(const AnyDomainRef& domain, AnyExprRef expr);

ParseResult parseExpr(nlohmann::json& essenceExpr, ParsedModel& parsedModel);
lib::optional<ParseResult> tryParseExpr(nlohmann::json& essenceExpr,
                                        ParsedModel& parsedModel);
AnyDomainRef parseDomain(nlohmann::json& essenceExpr, ParsedModel& parsedModel);
lib::optional<AnyDomainRef> tryParseDomain(nlohmann::json& domainExpr,
                                           ParsedModel& parsedModel);

MultiParseResult parseAllAsSameType(
    nlohmann::json& jsonArray, ParsedModel& parsedModel,
    std::function<nlohmann::json&(nlohmann::json&)> jsonMapper =
        [](nlohmann::json& j) -> nlohmann::json& { return j; });

template <typename Function,
          typename ReturnType = typename Function::result_type>
lib::optional<ReturnType> stringMatch(
    const HashMap<std::string, Function>& match, nlohmann::json& essenceExpr,
    ParsedModel& parsedModel) {
    for (auto jsonIter = essenceExpr.begin(); jsonIter != essenceExpr.end();
         ++jsonIter) {
        auto iter = match.find(jsonIter.key());
        if (iter != match.end()) {
            return iter->second(jsonIter.value(), parsedModel);
        }
    }
    return lib::nullopt;
}

template <typename RetType, typename Constraint, typename Func>
ExprRef<RetType> expect(Constraint&& constraint, Func&& func) {
    return lib::visit(
        overloaded([&](ExprRef<RetType>& constraint)
                       -> ExprRef<RetType> { return constraint; },
                   [&](auto&& constraint) -> ExprRef<RetType> {
                       typedef viewType(constraint) View;
                       typedef typename AssociatedValueType<View>::type Value;
                       if (std::is_same<EmptyView, View>::value) {
                           return OpMaker<OpUndefined<RetType>>::make();
                       }
                       func(constraint);
                       myCerr << "\nType found was instead "
                              << TypeAsString<Value>::value << std::endl;
                       myAbort();
                   }),
        constraint);
}
Int parseExprAsInt(AnyExprRef expr, const std::string& errorMessage);
Int parseExprAsInt(nlohmann::json& essenceExpr, ParsedModel& parsedModel,
                   const std::string& errorMessage);
struct SizeAttr;
SizeAttr parseSizeAttr(nlohmann::json& sizeAttrExpr, ParsedModel& parsedModel);
enum class PartialAttr;
PartialAttr parsePartialAttr(nlohmann::json& partialAttrExpr);
ParseResult toSequence(ParseResult parsedExpr);
#endif /* SRC_PARSING_PARSERCOMMON_H_ */
