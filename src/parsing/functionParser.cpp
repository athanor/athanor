#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/allVals.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
template <typename PreimageView, typename RangeView>
static void buildFunctionUsingDimensions(const AnyDomainRef& preimageDomain,
                                         FunctionView& functionView,
                                         bool partial,
                                         ExprRefVec<PreimageView>& domainExprs,
                                         ExprRefVec<RangeView>& rangeExprs) {
    functionView.initView(
        preimageDomain, makeDimensionVecFromDomain(preimageDomain),
        ExprRefVec<RangeView>(rangeExprs.size(), nullptr), partial);
    for (size_t i = 0; i < rangeExprs.size(); i++) {
        domainExprs[i]->evaluate();
        auto index = functionView.preimageToIndex(
            domainExprs[i]->getViewIfDefined().checkedGet(
                "Error: whilst parsing function lit, preimage is "
                "undefined."));
        if (!index) {
            myCerr << "Error in function literal: domain out of "
                      "range.\n";
            shouldNotBeCalledPanic;
        }
        functionView.template getRange<RangeView>()[*index] = rangeExprs[i];
    }
}
template <typename PreimageView, typename RangeView>
static void buildFunctionUsingExplicitPreimages(
    const AnyDomainRef& preimageDomain, FunctionView& functionView,
    bool partial, ExprRefVec<PreimageView>& domainExprs,
    ExprRefVec<RangeView>& rangeExprs) {
    ExplicitPreimageContainer preimages;
    preimages.preimages.emplace<ExprRefVec<PreimageView>>();
    for (auto& expr : domainExprs) {
        preimages.add(expr);
    }
    functionView.initView(preimageDomain, move(preimages), move(rangeExprs),
                          partial);
}

ParseResult parseOpFunctionLit(json& functionExpr, ParsedModel& parsedModel) {
    auto domainResult = parseAllAsSameType(
        functionExpr, parsedModel, [](json& j) -> json& { return j[0]; });
    auto rangeResult = parseAllAsSameType(
        functionExpr, parsedModel, [](json& j) -> json& { return j[1]; });
    if (!domainResult.allConstant) {
        myCerr << "Error: at the moment, do not support function literals "
                  "who's domain (preimages) "
                  "contain decision variables.  The co-domain (range) may "
                  "contain decision variables.\n";
        myExit(1);
    }
    return lib::visit(
        [&](auto& domainExprs, auto& rangeExprs) -> ParseResult {
            typedef viewType(rangeExprs) RangeView;
            bool partial =
                domainExprs.size() < getDomainSize(domainResult.domain);
            bool useDimensions =
                !partial && canBuildDimensionVec(domainResult.domain);
            auto op = OpMaker<OpFunctionLitBasic>::make<RangeView>(
                domainResult.domain);
            auto& functionView = *op->view();
            if (useDimensions) {
                buildFunctionUsingDimensions(domainResult.domain, functionView,
                                             partial, domainExprs, rangeExprs);
            } else {
                buildFunctionUsingExplicitPreimages(domainResult.domain,
                                                    functionView, partial,
                                                    domainExprs, rangeExprs);
            }
            auto domain = make_shared<FunctionDomain>(
                JectivityAttr::NONE, PartialAttr::TOTAL, domainResult.domain,
                rangeResult.domain);
            op->setConstant(rangeResult.allConstant);
            return ParseResult(domain, op, rangeResult.hasEmptyType);
        },
        domainResult.exprs, rangeResult.exprs);
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
ParseResult parseOpSequenceLit(json& matrixExpr, ParsedModel& parsedModel);
shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel);

ParseResult parseOpMatrixLit(json& matrixExpr, ParsedModel& parsedModel) {
    auto domain = parseDomainInt(matrixExpr[0]["DomainInt"], parsedModel);
    if (domain->bounds.size() == 1 && domain->bounds.front().first == 1) {
        return parseOpSequenceLit(matrixExpr[1], parsedModel);
    }

    auto rangeResult = parseAllAsSameType(matrixExpr[1], parsedModel);
    return lib::visit(
        [&](auto& rangeExprs) -> ParseResult {
            typedef viewType(rangeExprs) View;
            auto op = OpMaker<OpFunctionLitBasic>::make<View>(domain);
            auto& functionView = *op->view();
            functionView.initView(domain, makeDimensionVecFromDomain(domain),
                                  rangeExprs, false);
            auto functionDomain = make_shared<FunctionDomain>(
                JectivityAttr::NONE, PartialAttr::TOTAL, domain,
                rangeResult.domain);
            functionDomain->isMatrixDomain = true;
            op->setConstant(rangeResult.allConstant);
            return ParseResult(functionDomain, op, rangeResult.hasEmptyType);
        },
        rangeResult.exprs);
}
