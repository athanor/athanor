#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/allVals.h"
using namespace std;
using namespace lib;
using namespace nlohmann;

ParseResult parseOpFunctionLit(json& functionExpr, ParsedModel& parsedModel) {
    auto domainResult = parseAllAsSameType(
        functionExpr, parsedModel, [](json& j) -> json& { return j[0]; });
    auto rangeResult = parseAllAsSameType(
        functionExpr, parsedModel, [](json& j) -> json& { return j[1]; });
    if (!domainResult.allConstant || !rangeResult.allConstant) {
        cerr << "Error: at the moment, do not support function literals with "
                "decision variables.\n";
    }
    auto function = make<FunctionValue>();
    mpark::visit(
        [&](auto& domainExprs, auto& rangeExprs) {
            typedef viewType(rangeExprs) View;
            typedef typename AssociatedValueType<View>::type Value;
            function->resetDimensions<View>(
                domainResult.domain,
                FunctionView::makeDimensionVecFromDomain(domainResult.domain));
            for (size_t i = 0; i < rangeExprs.size(); i++) {
                if (!domainExprs[i]->isConstant()) {
                    cerr << "Error: function literals only support constants "
                            "at the moment.\n";
                    exit(1);
                }
                domainExprs[i]->evaluate();
                auto index = function->domainToIndex(
                    domainExprs[i]->getViewIfDefined().checkedGet(
                        NO_FUNCTION_UNDEFINED_MEMBERS));
                if (!index) {
                    cerr << "Error in function literal: domain out of range.\n";
                    abort();
                }
                if (!getAs<Value>(rangeExprs[i])) {
                    cerr << "Error: function literals currently only "
                            "support literals, expressions not allowed.\n";
                    exit(1);
                }
                function->assignImage<Value>(*index,
                                             assumeAsValue(rangeExprs[i]));
            }
        },
        domainResult.exprs, rangeResult.exprs);
    function->setConstant(true);
    auto domain =
        make_shared<FunctionDomain>(JectivityAttr::NONE, PartialAttr::TOTAL,
                                    domainResult.domain, rangeResult.domain);
    return ParseResult(domain, function.asExpr(), false);
}

shared_ptr<FunctionDomain> parseDomainFunction(json& functionDomainExpr,
                                               ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(functionDomainExpr[1][0], parsedModel);

    if (sizeAttr.sizeType != SizeAttr::SizeAttrType::NO_SIZE) {
        cerr << "Do not support/understand function with "
                "size attribute.\nn";
        abort();
    }
    PartialAttr partialAttr = parsePartialAttr(functionDomainExpr[1][1]);
    if (partialAttr != PartialAttr::TOTAL) {
        cerr << "Error: currently only support total "
                "functions.\n";
        abort();
    }
    if (functionDomainExpr[1][2] != "JectivityAttr_None") {
        cerr << "Error, not supporting jectivity attributes "
                "on functions at "
                "the moment.\n";
        abort

            ();
    }
    return make_shared<FunctionDomain>(
        JectivityAttr::NONE, partialAttr,
        parseDomain(functionDomainExpr[2], parsedModel),
        parseDomain(functionDomainExpr[3], parsedModel));
}

ParseResult parseOpFunctionImage(AnyDomainRef& innerDomain,
                                 ExprRef<FunctionView>& function,
                                 json& preImageExpr, bool hasEmptyType,
                                 ParsedModel& parsedModel) {
    auto preImage = parseExpr(preImageExpr[0], parsedModel).expr;
    return mpark::visit(
        [&](auto& innerDomain, auto& preImage) {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<InnerDomainType>::type View;
            bool constant = function->isConstant() && preImage->isConstant();
            auto op = OpMaker<OpFunctionImage<View>>::make(function, preImage);
            op->setConstant(constant);
            return ParseResult(innerDomain, op, hasEmptyType);
        },
        innerDomain, preImage);
}
