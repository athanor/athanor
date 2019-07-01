#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/tupleVal.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
ParseResult parseOpRecordLit(json& tupleExpr, ParsedModel& parsedModel) {
    bool constant = true;
    bool hasEmptyType = false;
    vector<tuple<string, AnyDomainRef, AnyExprRef>> fields;
    for (auto& memberExpr : tupleExpr) {
        auto member = parseExpr(memberExpr[1], parsedModel);
        fields.emplace_back(memberExpr[0]["Name"], member.domain, member.expr);
        mpark::visit([&](auto& member) { constant &= member->isConstant(); },
                     member.expr);
        hasEmptyType |= member.hasEmptyType;
    }
    sort(fields.begin(), fields.end(),
         [](auto& a, auto& b) { return get<0>(a) < get<0>(b); });
    vector<AnyDomainRef> innerDomains;
    vector<string> recordIndexNameMap;
    HashMap<string, size_t> recordNameIndexMap;
    vector<AnyExprRef> tupleMembers;
    for (auto& field : fields) {
        innerDomains.emplace_back(move(get<1>(field)));
        recordIndexNameMap.emplace_back(get<0>(field));
        recordNameIndexMap[get<0>(field)] = recordIndexNameMap.size() - 1;
        tupleMembers.emplace_back(move(get<2>(field)));
    }
    auto domain = make_shared<TupleDomain>(move(innerDomains));
    domain->isRecord = true;
    domain->recordIndexNameMap = move(recordIndexNameMap);
    domain->recordNameIndexMap = move(recordNameIndexMap);
    auto tuple = OpMaker<OpTupleLit>::make(move(tupleMembers));
    tuple->setConstant(constant);
    return ParseResult(domain, tuple, hasEmptyType);
}

ParseResult parseOpTupleLit(json& tupleExpr, ParsedModel& parsedModel) {
    bool constant = true;
    bool hasEmptyType = false;
    vector<AnyDomainRef> tupleMemberDomains;
    vector<AnyExprRef> tupleMembers;
    for (auto& memberExpr : tupleExpr) {
        auto member = parseExpr(memberExpr, parsedModel);
        tupleMemberDomains.emplace_back(member.domain);
        tupleMembers.emplace_back(member.expr);
        mpark::visit([&](auto& member) { constant &= member->isConstant(); },
                     member.expr);
        hasEmptyType |= member.hasEmptyType;
    }
    auto tuple = OpMaker<OpTupleLit>::make(move(tupleMembers));
    tuple->setConstant(constant);
    auto domain = make_shared<TupleDomain>(move(tupleMemberDomains));
    return ParseResult(domain, tuple, hasEmptyType);
}

shared_ptr<TupleDomain> parseDomainTuple(json& tupleDomainExpr,
                                         ParsedModel& parsedModel) {
    vector<AnyDomainRef> innerDomains;
    for (auto& innerDomainExpr : tupleDomainExpr) {
        innerDomains.emplace_back(parseDomain(innerDomainExpr, parsedModel));
    }
    return make_shared<TupleDomain>(move(innerDomains));
}

shared_ptr<TupleDomain> parseDomainRecord(json& tupleDomainExpr,
                                          ParsedModel& parsedModel) {
    vector<pair<string, AnyDomainRef>> fields;
    for (auto& innerDomainExpr : tupleDomainExpr) {
        fields.emplace_back(innerDomainExpr[0]["Name"],
                            parseDomain(innerDomainExpr[1], parsedModel));
    }
    sort(fields.begin(), fields.end(),
         [](auto& a, auto& b) { return a.first < b.first; });
    vector<AnyDomainRef> innerDomains;
    vector<string> recordIndexNameMap;
    HashMap<string, size_t> recordNameIndexMap;
    for (auto& field : fields) {
        innerDomains.emplace_back(move(field.second));
        recordIndexNameMap.emplace_back(field.first);
        recordNameIndexMap[field.first] = recordIndexNameMap.size() - 1;
    }
    auto domain = make_shared<TupleDomain>(move(innerDomains));
    domain->isRecord = true;
    domain->recordIndexNameMap = move(recordIndexNameMap);
    domain->recordNameIndexMap = move(recordNameIndexMap);
    return domain;
}

ParseResult parseOpTupleIndex(shared_ptr<TupleDomain>& tupleDomain,
                              ExprRef<TupleView>& tuple, json& indexExpr,
                              ParsedModel& parsedModel) {
    string errorMessage = "within tuple index expression.";
    UInt index = parseExprAsInt(indexExpr, parsedModel, errorMessage) - 1;
    if (index >= tupleDomain->inners.size()) {
        cerr << "Error: tuple index out of range.\n";
        exit(1);
    }
    bool hasEmptyType = hasEmptyDomain(tupleDomain->inners[index]);
    return mpark::visit(
        [&](auto& innerDomain) -> ParseResult {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<InnerDomainType>::type View;
            bool constant = tuple->isConstant();
            auto op = OpMaker<OpTupleIndex<View>>::make(tuple, index);
            op->setConstant(constant);
            return ParseResult(innerDomain, op, hasEmptyType);
        },
        tupleDomain->inners[index]);
}

ParseResult parseOpRecordIndex(shared_ptr<TupleDomain>& tupleDomain,
                               ExprRef<TupleView>& tuple, json& indexExpr,
                               ParsedModel&) {
    string errorMessage = "within record index expression.";
    string indexName = indexExpr["Reference"][0]["Name"];
    if (!tupleDomain->recordNameIndexMap.count(indexName)) {
        cerr << "Error: could not index this record by the name " << indexName
             << endl;
        exit(1);
    }
    size_t index = tupleDomain->recordNameIndexMap[indexName];
    bool hasEmptyType = hasEmptyDomain(tupleDomain->inners[index]);
    return mpark::visit(
        [&](auto& innerDomain) -> ParseResult {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<InnerDomainType>::type View;
            bool constant = tuple->isConstant();
            auto op = OpMaker<OpTupleIndex<View>>::make(tuple, index);
            op->setConstant(constant);
            return ParseResult(innerDomain, op, hasEmptyType);
        },
        tupleDomain->inners[index]);
}
