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
        myCerr << "Error: at the moment, do not support function literals with "
                  "decision variables.\n";
    }
    auto function = make<FunctionValue>();
    lib::visit(
        [&](auto& domainExprs, auto& rangeExprs) {
            typedef viewType(rangeExprs) View;
            typedef typename AssociatedValueType<View>::type Value;
            function->resetDimensions<View>(
                domainResult.domain,
                FunctionView::makeDimensionVecFromDomain(domainResult.domain));
            for (size_t i = 0; i < rangeExprs.size(); i++) {
                if (!domainExprs[i]->isConstant()) {
                    myCerr << "Error: function literals only support constants "
                              "at the moment.\n";
                    myExit(1);
                }
                domainExprs[i]->evaluate();
                auto index = function->domainToIndex(
                    domainExprs[i]->getViewIfDefined().checkedGet(
                        NO_FUNCTION_UNDEFINED_MEMBERS));
                if (!index) {
                    myCerr
                        << "Error in function literal: domain out of range.\n";
                    myAbort();
                }
                if (!getAs<Value>(rangeExprs[i])) {
                    myCerr << "Error: function literals currently only "
                              "support literals, expressions not allowed.\n";
                    myExit(1);
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
        myCerr << "Do not support/understand function with "
                  "size attribute.\nn";
        myAbort();
    }
    PartialAttr partialAttr = parsePartialAttr(functionDomainExpr[1][1]);
    if (partialAttr != PartialAttr::TOTAL) {
        myCerr << "Error: currently only support total "
                  "functions.\n";
        myAbort();
    }
    if (functionDomainExpr[1][2] != "JectivityAttr_None") {
        myCerr << "Error, not supporting jectivity attributes "
                  "on functions at "
                  "the moment.\n";
        myAbort

            ();
    }
    return make_shared<FunctionDomain>(
        JectivityAttr::NONE, partialAttr,
        parseDomain(functionDomainExpr[2], parsedModel),
        parseDomain(functionDomainExpr[3], parsedModel));
}

ParseResult parseOpFunctionImage(const FunctionDomain& domain,
                                 ExprRef<FunctionView>& function,
                                 json& preImageExpr, bool hasEmptyType,
                                 ParsedModel& parsedModel) {
    auto preImage = parseExpr(preImageExpr, parsedModel).expr;
    return lib::visit(
        [&](auto& innerDomain, auto& preImage) {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<InnerDomainType>::type View;
            typedef viewType(preImage) preImageViewType;
            typedef typename AssociatedDomain<preImageViewType>::type
                preImageDomain;
            if (!lib::get_if<shared_ptr<preImageDomain>>(&domain.from)) {
                myCerr << "Miss match in pre image domain and function domain "
                          "when parsing OpFunctionImage.\nfunction:"
                       << domain << "\npre image type: "
                       << TypeAsString<typename AssociatedValueType<
                              preImageDomain>::type>::value
                       << endl;
                myAbort();
            }
            bool constant = function->isConstant() && preImage->isConstant();
            auto op = OpMaker<OpFunctionImage<View>>::make(function, preImage);
            op->setConstant(constant);
            return ParseResult(innerDomain, op, hasEmptyType);
        },
        domain.to, preImage);
}

shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel);

shared_ptr<FunctionDomain> parseDomainMatrix(json& matrixDomainExpr,
                                             ParsedModel& parsedModel) {
    auto indexingDomain = parseDomain(matrixDomainExpr[0], parsedModel);
    auto indexingDomainIntTest =
        lib::get_if<shared_ptr<IntDomain>>(&indexingDomain);
    if (!indexingDomainIntTest) {
        myCerr << "Error: matrices must be indexed by int domains.\n";
        myAbort();
    }
    return FunctionDomain::makeMatrixDomain(
        *indexingDomainIntTest, parseDomain(matrixDomainExpr[1], parsedModel));
}
