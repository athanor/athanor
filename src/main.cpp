
#include <autoArgParse/argParser.h>
#include <iostream>
#include <unordered_map>
#include "common/common.h"

#include <fstream>
#include <json.hpp>
#include "constructorShortcuts.h"
#include "search/searchStrategies.h"
#include "types/typeOperations.h"

using namespace std;
void setOfSetWithModulous();
void setWithModulous();
void mSetOfSetWithModulousAndMaxSizeSum();
void sonet(const int argc, const char** argv);
using namespace AutoArgParse;
ArgParser argParser;
auto& fileArg = argParser.add<Arg<ifstream>>(
    "path_to_file", Policy::MANDATORY, "Path to parameter file.",
    [](const string& path) -> ifstream {
        ifstream file;
        file.open(path);
        if (file.good()) {
            return file;
        } else {
            throw ErrorMessage("Error opening file: " + path);
        }
    });
auto& randomSeedFlag = argParser.add<ComplexFlag>(
    "--randomSeed", Policy::OPTIONAL, "Specify a random seed.",
    [](const string&) { useSeedForRandom = true; });
auto& seedArg = randomSeedFlag.add<Arg<int>>(
    "integer_seed", Policy::MANDATORY,
    "Integer seed to use for random generator.",
    chain(Converter<int>(), [](int value) {
        if (value < 0) {
            throw ErrorMessage("Seed must be greater or equal to 0.");
        } else {
            seedForRandom = (u_int64_t)(value);
            return value;
        }
    }));
void testHashes() {
    const size_t max = 10000;
    unordered_map<u_int64_t, pair<u_int64_t, u_int64_t>> seenHashes;
    for (u_int64_t i = 0; i < max; ++i) {
        for (u_int64_t j = i; j < max; ++j) {
            u_int64_t hashSum = mix(i) + mix(j);
            if (seenHashes.count(hashSum)) {
                cerr << "Error, found collision:\n";
                cerr << "mix(" << i << ") + mix(" << j << ") collides with ";
                auto& pair = seenHashes[hashSum];
                cerr << "mix(" << pair.first << ") + mix(" << pair.second
                     << ")\nBoth produce total hash of " << hashSum << endl;
                exit(1);
            }
        }
    }
}
void jsonTest(const int argc, const char** argv);
int main(const int argc, const char** argv) {
    // sonet(argc, argv);
    jsonTest(argc, argv);
}

void setOfSetWithModulous() {
    auto zeroConst = ValRef<IntValue>(make_shared<IntValue>());
    zeroConst->value = 0;
    auto modulousConst = ValRef<IntValue>(make_shared<IntValue>());
    modulousConst->value = 2;
    auto sizeLimitConst = ValRef<IntValue>(make_shared<IntValue>());
    sizeLimitConst->value = 10;
    ModelBuilder builder;

    auto domain = make_shared<SetDomain>(
        minSize(1), SetDomain(noSize(), IntDomain({intBound(1, 40)})));
    auto a = builder.addVariable(domain);
    builder.addConstraint(intEq(setSize(a), sizeLimitConst));

    auto outerQuant = quant<BoolReturning>(a);
    auto outerForall = opAnd(outerQuant);
    auto i = outerQuant->newIterRef<SetValue>();
    auto innerQuant = quant<BoolReturning>(i);
    auto innerForAll = opAnd(innerQuant);
    outerQuant->setExpression(
        opAnd(intEq(setSize(i), sizeLimitConst), innerForAll));
    auto j = innerQuant->newIterRef<IntValue>();
    innerQuant->setExpression(intEq(mod(j, modulousConst), zeroConst));
    builder.addConstraint(outerForall);
    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}

void setWithModulous() {
    auto zeroConst = ValRef<IntValue>(make_shared<IntValue>());
    zeroConst->value = 0;
    auto modulousConst = ValRef<IntValue>(make_shared<IntValue>());
    modulousConst->value = 2;
    auto sizeLimitConst = ValRef<IntValue>(make_shared<IntValue>());
    sizeLimitConst->value = 4;
    ModelBuilder builder;

    auto domain =
        make_shared<SetDomain>(minSize(1), IntDomain({intBound(1, 3)}));
    auto a = builder.addVariable(domain);
    builder.addConstraint(intEq(setSize(a), sizeLimitConst));

    auto outerQuant = quant<BoolReturning>(a);
    auto outerForall = opAnd(outerQuant);
    auto i = outerQuant->newIterRef<IntValue>();
    outerQuant->setExpression(intEq(mod(i, modulousConst), zeroConst));
    builder.addConstraint(outerForall);
    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}

void mSetOfSetWithModulousAndMaxSizeSum() {
    auto zeroConst = ValRef<IntValue>(make_shared<IntValue>());
    zeroConst->value = 0;
    auto modulousConst = ValRef<IntValue>(make_shared<IntValue>());
    modulousConst->value = 2;
    ModelBuilder builder;

    auto domain = make_shared<MSetDomain>(
        exactSize(2), SetDomain(noSize(), IntDomain({intBound(1, 20)})));
    auto a = builder.addVariable(domain);

    auto outerQuant = quant<BoolReturning>(a);
    auto outerForall = opAnd(outerQuant);
    auto i = outerQuant->newIterRef<SetValue>();
    auto innerQuant = quant<BoolReturning>(i);
    auto innerForAll = opAnd(innerQuant);
    outerQuant->setExpression(innerForAll);
    auto j = innerQuant->newIterRef<IntValue>();
    innerQuant->setExpression(intEq(mod(j, modulousConst), zeroConst));
    builder.addConstraint(outerForall);
    auto outerIntQuant = quant<IntReturning>(a);
    auto outerSum = sum(outerIntQuant);
    auto k = outerIntQuant->newIterRef<SetValue>();
    outerIntQuant->setExpression(setSize(k));
    builder.setObjective(OptimiseMode::MAXIMISE, outerSum);
    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}

void sonet(const int argc, const char** argv) {
    argParser.validateArgs(argc, argv);
    auto& is = fileArg.get();
    u_int64_t numberNodes, numberRings, capacity;
    is >> numberNodes;
    is >> numberRings;
    is >> capacity;
    auto nodesDomain = make_shared<IntDomain>(
        vector<pair<int64_t, int64_t>>({intBound(1, numberNodes)}));
    auto innerSetDomain = make_shared<SetDomain>(exactSize(2), nodesDomain);
    auto demandConst =
        constructValueFromDomain(SetDomain(noSize(), innerSetDomain));
    demandConst->container = &constantPool;
    u_int64_t a, b;
    while (is && is >> a && is && is >> b) {
        auto innerSet = constructValueFromDomain(*innerSetDomain);
        auto aVal = constructValueFromDomain(*nodesDomain);
        auto bVal = constructValueFromDomain(*nodesDomain);
        aVal->value = a;
        bVal->value = b;
        innerSet->addMember(aVal);
        innerSet->addMember(bVal);
        demandConst->addMember(innerSet);
    }
    cout << "Input:\n";
    cout << "letting numberNodes be " << numberNodes << endl;
    cout << "letting numberRings be " << numberRings << endl;
    cout << "letting capacity be " << capacity << endl;
    cout << "letting demand be " << *demandConst << endl;
    ModelBuilder builder;

    auto mSetDomain = make_shared<MSetDomain>(
        exactSize(numberRings), SetDomain(maxSize(capacity), nodesDomain));
    auto networkVar = builder.addVariable(mSetDomain);
    auto outerIntQuant = quant<IntReturning>(networkVar);
    auto outerSum = sum(outerIntQuant);
    auto k = outerIntQuant->newIterRef<SetValue>();
    outerIntQuant->setExpression(setSize(k));
    builder.setObjective(OptimiseMode::MINIMISE, outerSum);
    // constraints
    auto outerQuant = quant<BoolReturning>(demandConst);
    auto forAll = opAnd(outerQuant);
    auto i = outerQuant->newIterRef<SetValue>();
    auto innerQuant = quant<BoolReturning>(networkVar);
    auto exists = opOr(innerQuant);
    outerQuant->setExpression(exists);
    auto j = innerQuant->newIterRef<SetValue>();
    innerQuant->setExpression(subset(i, j));
    builder.addConstraint(forAll);
    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}
using namespace nlohmann;
struct ParsedModel {
    ModelBuilder builder;
    unordered_map<string, AnyValRef> vars;
    unordered_map<string, AnyDomainRef> domainLettings;
    unordered_map<string, AnyOpRef> constantExprs;
};

pair<bool, AnyValRef> tryParseValue(json& essenceExpr,
                                    ParsedModel& parsedModel);
pair<bool, AnyDomainRef> tryParseDomain(json& essenceExpr,
                                        ParsedModel& parsedModel);

AnyValRef parseValue(json& essenceExpr, ParsedModel& parsedModel) {
    auto boolValuePair = tryParseValue(essenceExpr, parsedModel);
    if (boolValuePair.first) {
        return move(boolValuePair.second);
    } else {
        cerr << "Failed to parse value: " << essenceExpr << endl;
        abort();
    }
}

int64_t parseValueAsInt(json& essenceExpr, ParsedModel& parsedModel) {
    return mpark::get<ValRef<IntValue>>(parseValue(essenceExpr, parsedModel))
        ->value;
}

AnyDomainRef parseDomain(json& essenceExpr, ParsedModel& parsedModel) {
    auto boolDomainPair = tryParseDomain(essenceExpr, parsedModel);
    if (boolDomainPair.first) {
        return move(boolDomainPair.second);
    } else {
        cerr << "Failed to parse domain: " << essenceExpr << endl;
        abort();
    }
}

ValRef<SetValue> parseConstantSet(json& essenceSetConstant,
                                  ParsedModel& parsedModel) {
    ValRef<SetValue> val = make<SetValue>();
    // just in case set is empty, not handling the case but at least leaving
    // inner type not undefined  assertions will catch it later
    if (essenceSetConstant.size() == 0) {
        val->setInnerType<IntValue>();
        cerr << "Not sure how to work out type of empty set yet, will handle "
                "this later.";
        todoImpl();
    }
    mpark::visit(
        [&](auto&& firstMember) {
            typedef valType(firstMember) InnerValueType;
            val->setInnerType<InnerValueType>();
            val->addMember(firstMember);
            for (size_t i = 1; i < essenceSetConstant.size(); ++i) {
                val->addMember(mpark::get<ValRef<InnerValueType>>(
                    parseValue(essenceSetConstant[i], parsedModel)));
            }
        },
        parseValue(essenceSetConstant[0], parsedModel));
    return val;
}

AnyValRef parseAbstractLiteral(json& abstractLit, ParsedModel& parsedModel) {
    if (abstractLit.count("AbsLitSet")) {
        return parseConstantSet(abstractLit["AbsLitSet"], parsedModel);
    } else {
        cerr << "Not sure what type of abstract literal this is: "
             << abstractLit << endl;
        abort();
    }
}

AnyValRef parseConstant(json& essenceConstant, ParsedModel&) {
    if (essenceConstant.count("ConstantInt")) {
        auto val = make<IntValue>();
        val->value = essenceConstant["ConstantInt"];
        return val;
    } else if (essenceConstant.count("ConstantBool")) {
        auto val = make<BoolValue>();
        val->violation = bool(essenceConstant["ConstantBool"]);
        return val;
    } else {
        cerr << "Not sure what type of constant this is: " << essenceConstant
             << endl;
        abort();
    }
}

pair<bool, AnyValRef> tryParseValue(json& essenceExpr,
                                    ParsedModel& parsedModel) {
    if (essenceExpr.count("Constant")) {
        return make_pair(true,
                         parseConstant(essenceExpr["Constant"], parsedModel));
    } else if (essenceExpr.count("AbstractLiteral")) {
        return make_pair(
            true,
            parseAbstractLiteral(essenceExpr["AbstractLiteral"], parsedModel));
    } else if (essenceExpr.count("Reference")) {
        auto& essenceReference = essenceExpr["Reference"];
        string referenceName = essenceReference[0]["Name"];
        if (!parsedModel.vars.count(referenceName)) {
            cerr << "Found reference to value with name \"" << referenceName
                 << "\" but this does not appear to be in scope.\n"
                 << essenceExpr << endl;
            abort();
        } else {
            return make_pair(true, parsedModel.vars.at(referenceName));
        }
    }
    return make_pair(false, AnyValRef(ValRef<IntValue>(nullptr)));
}

shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel) {
    vector<pair<int64_t, int64_t>> ranges;
    for (auto& rangeExpr : intDomainExpr) {
        int64_t from, to;

        if (rangeExpr.count("RangeBounded")) {
            from = parseValueAsInt(rangeExpr["RangeBounded"][0], parsedModel);
            to = parseValueAsInt(rangeExpr["RangeBounded"][1], parsedModel);
        } else if (rangeExpr.count("RangeSingle")) {
            from = parseValueAsInt(rangeExpr["RangeSingle"], parsedModel);
            to = from;
        } else {
            cerr << "Unrecognised type of int range: " << rangeExpr << endl;
            abort();
        }
        ranges.emplace_back(from, to);
    }
    return make_shared<IntDomain>(move(ranges));
}

shared_ptr<BoolDomain> parseDomainBool(json&, ParsedModel&) {
    return make_shared<BoolDomain>();
}

SizeAttr parseSizeAttr(json& sizeAttrExpr, ParsedModel& parsedModel) {
    if (sizeAttrExpr.count("SizeAttr_None")) {
        return noSize();
    } else if (sizeAttrExpr.count("SizeAttr_MinSize")) {
        return minSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_MinSize"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_MaxSize")) {
        return maxSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_MaxSize"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_Size")) {
        return exactSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_Size"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_MinMaxSize")) {
        auto& sizeRangeExpr = sizeAttrExpr["SizeAttr_MinMaxSize"];
        return sizeRange(parseValueAsInt(sizeRangeExpr[0], parsedModel),
                         parseValueAsInt(sizeRangeExpr[1], parsedModel));
    } else {
        cerr << "Could not parse this as a size attribute: " << sizeAttrExpr
             << endl;
        abort();
    }
}

shared_ptr<SetDomain> parseDomainSet(json& setDomainExpr,
                                     ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(setDomainExpr[1], parsedModel);
    return make_shared<SetDomain>(sizeAttr,
                                  parseDomain(setDomainExpr[2], parsedModel));
}

shared_ptr<MSetDomain> parseDomainMSet(json& mSetDomainExpr,
                                       ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(mSetDomainExpr[1][0], parsedModel);
    if (!mSetDomainExpr[1][1].count("OccurAttr_None")) {
        cerr << "Error: for the moment, given attribute must be "
                "OccurAttr_None.  This is not handled yet: "
             << mSetDomainExpr[1][1] << endl;
        abort();
    }
    return make_shared<MSetDomain>(sizeAttr,
                                   parseDomain(mSetDomainExpr[2], parsedModel));
}

pair<bool, AnyDomainRef> tryParseDomain(json& domainExpr,
                                        ParsedModel& parsedModel) {
    if (domainExpr.count("DomainInt")) {
        return make_pair(true,
                         parseDomainInt(domainExpr["DomainInt"], parsedModel));
    } else if (domainExpr.count("DomainBool")) {
        return make_pair(
            true, parseDomainBool(domainExpr["DomainBool"], parsedModel));
    } else if (domainExpr.count("DomainSet")) {
        return make_pair(true,
                         parseDomainSet(domainExpr["DomainSet"], parsedModel));
    } else if (domainExpr.count("DomainMSet")) {
        return make_pair(
            true, parseDomainMSet(domainExpr["DomainMSet"], parsedModel));
    } else if (domainExpr.count("DomainReference")) {
        auto& domainReference = domainExpr["DomainReference"];
        string referenceName = domainReference[0]["Name"];
        if (!parsedModel.domainLettings.count(referenceName)) {
            cerr << "Found reference to domainwith name \"" << referenceName
                 << "\" but this does not appear to be in scope.\n"
                 << domainExpr << endl;
            abort();
        } else {
            return make_pair(true,
                             parsedModel.domainLettings.at(referenceName));
        }
    }
    return make_pair(false, AnyDomainRef(shared_ptr<IntDomain>(nullptr)));
}

void handleLettingDeclaration(json& lettingArray, ParsedModel& parsedModel) {
    string lettingName = lettingArray[0]["Name"];
    auto boolValuePair = tryParseValue(lettingArray[1], parsedModel);
    if (boolValuePair.first) {
        parsedModel.vars.emplace(lettingName, boolValuePair.second);
        return;
    }
    if (lettingArray[1].count("Domain")) {
        auto domain = parseDomain(lettingArray[1]["Domain"], parsedModel);
        parsedModel.domainLettings.emplace(lettingName, domain);
        return;
    }
    cerr << "Not sure how to parse this letting: " << lettingArray << endl;
}

void jsonTest(const int argc, const char** argv) {
    argParser.validateArgs(argc, argv);
    auto& is = fileArg.get();
    json j;
    is >> j;
    ParsedModel parsedModel;
    for (auto& statement : j["mStatements"]) {
        if (statement.count("Declaration") &&
            statement["Declaration"].count("Letting")) {
            handleLettingDeclaration(statement["Declaration"]["Letting"],
                                     parsedModel);
        }
    }
    cout << parsedModel.vars << endl;
    cout << parsedModel.domainLettings << endl;
}
