#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/allVals.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
shared_ptr<EnumDomain> parseDomainEnum(json& enumDomainExpr,
                                       ParsedModel& parsedModel) {
    string name = enumDomainExpr[0]["Name"];
    auto iter = parsedModel.domainLettings.find(name);
    if (iter == parsedModel.domainLettings.end()) {
        cerr << "Could Not find enum " << name << endl;
        exit(1);
    }
    return mpark::get<shared_ptr<EnumDomain>>(iter->second);
}

void handleEnumDeclaration(json& enumDeclExpr, ParsedModel& parsedModel) {
    const string& enumName = enumDeclExpr[0]["Name"];
    vector<string> valueNames;
    for (auto& nameExpr : enumDeclExpr[1]) {
        string name = nameExpr["Name"];
        valueNames.emplace_back(move(name));
    }
    auto enumDomain = make_shared<EnumDomain>(enumName, move(valueNames));
    parsedModel.domainLettings.emplace(enumName, enumDomain);
    for (size_t i = 0; i < enumDomain->valueNames.size(); i++) {
        auto val = make<EnumValue>();
        val->value = i;
        val->setConstant(true);
        const string& name = enumDomain->valueNames[i];
        bool inserted =
            parsedModel.namedExprs
                .emplace(name, ParseResult(enumDomain, val.asExpr(), false))
                .second;
        if (!inserted) {
            cerr << "Error: whilst defining enum domain " << enumName
                 << ", the name " << name << " has already been used.\n";
            abort();
        }
    }
}
