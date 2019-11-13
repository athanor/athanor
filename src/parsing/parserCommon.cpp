#include "parsing/parserCommon.h"
#include <algorithm>
#include "types/allVals.h"
using namespace std;
using namespace lib;
using namespace nlohmann;

template <typename T>
void mergeIfSameType(shared_ptr<T>& dest, shared_ptr<T>& src) {
    if (is_same<EmptyDomain, T>::value) {
        return;
    }
    dest->merge(*src);
}
template <typename T, typename U>
void mergeIfSameType(shared_ptr<T>&, shared_ptr<U>&) {
    if (is_same<EmptyDomain, U>::value) {
        return;
    }
    typedef typename AssociatedValueType<T>::type Type1;
    typedef typename AssociatedValueType<U>::type Type2;
    myCerr << "Cannot merge domains for type " << TypeAsString<Type1>::value
           << " and " << TypeAsString<Type2>::value << ".\n";
    myAbort();
}

void mergeDomains(AnyDomainRef& dest, AnyDomainRef& src) {
    auto overload = overloaded(
        [&](shared_ptr<EmptyDomain>&, auto& src) { dest = src->deepCopy(src); },
        [](auto& dest, auto& src) { mergeIfSameType(dest, src); });
    lib::visit(overload, dest, src);
}

MultiParseResult parseAllAsSameType(json& jsonArray, ParsedModel& parsedModel,
                                    function<json&(json&)> jsonMapper) {
    vector<size_t> indicesOfEmpties;
    MultiParseResult result;
    result.exprs.emplace<ExprRefVec<EmptyView>>();
    for (size_t i = 0; i < jsonArray.size(); i++) {
        auto member = parseExpr(jsonMapper(jsonArray[i]), parsedModel);
        if (i == 0) {
            result.domain = deepCopyDomain(member.domain);
        } else {
            mergeDomains(result.domain, member.domain);
        }
        lib::visit(
            [&](auto& expr) {
                auto& exprs =
                    (i == 0)
                        ? result.exprs.emplace<ExprRefVec<viewType(expr)>>()
                        : lib::get<ExprRefVec<viewType(expr)>>(result.exprs);
                exprs.emplace_back(expr);
                result.allConstant &= expr->isConstant();
            },
            member.expr);
        if (member.hasEmptyType) {
            indicesOfEmpties.emplace_back(i);
        }
    }
    for (auto index : indicesOfEmpties) {
        lib::visit(
            [&](auto& exprs) {
                tryRemoveEmptyType(result.domain, exprs[index]);
            },
            result.exprs);
    }
    result.hasEmptyType = hasEmptyDomain(result.domain);
    return result;
}

SizeAttr parseSizeAttr(json& sizeAttrExpr, ParsedModel& parsedModel) {
    string errorMessage = "within size attribute";
    if (sizeAttrExpr.count("SizeAttr_None")) {
        return noSize();
    } else if (sizeAttrExpr.count("SizeAttr_MinSize")) {
        return minSize(parseExprAsInt(sizeAttrExpr["SizeAttr_MinSize"],
                                      parsedModel, errorMessage));
    } else if (sizeAttrExpr.count("SizeAttr_MaxSize")) {
        return maxSize(parseExprAsInt(sizeAttrExpr["SizeAttr_MaxSize"],
                                      parsedModel, errorMessage));
    } else if (sizeAttrExpr.count("SizeAttr_Size")) {
        return exactSize(parseExprAsInt(sizeAttrExpr["SizeAttr_Size"],
                                        parsedModel, errorMessage));
    } else if (sizeAttrExpr.count("SizeAttr_MinMaxSize")) {
        auto& sizeRangeExpr = sizeAttrExpr["SizeAttr_MinMaxSize"];
        return sizeRange(
            parseExprAsInt(sizeRangeExpr[0], parsedModel, errorMessage),
            parseExprAsInt(sizeRangeExpr[1], parsedModel, errorMessage));
    } else {
        myCerr << "Could not parse this as a size attribute: " << sizeAttrExpr
               << endl;
        myAbort();
    }
}
PartialAttr parsePartialAttr(json& partialAttrExpr) {
    if (partialAttrExpr == "PartialityAttr_Partial") {
        return PartialAttr::PARTIAL;
    } else if (partialAttrExpr == "PartialityAttr_Total") {
        return PartialAttr::TOTAL;
    } else {
        myCerr << "Error: unknown partiality attribute: " << partialAttrExpr
               << endl;
        myAbort();
    }
}
