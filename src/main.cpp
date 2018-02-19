
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
using namespace AutoArgParse;
using namespace nlohmann;

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
void parseModel(const int argc, const char** argv);
int main(const int argc, const char** argv) {
    // sonet(argc, argv);
    parseModel(argc, argv);
}

struct ParsedModel {
    ModelBuilder builder;
    unordered_map<string, AnyValRef> vars;
    unordered_map<string, AnyDomainRef> domainLettings;
    unordered_map<string, AnyExprRef> constantExprs;
};

pair<bool, AnyValRef> tryParseValue(json& essenceExpr,
                                    ParsedModel& parsedModel);
pair<bool, AnyDomainRef> tryParseDomain(json& essenceExpr,
                                        ParsedModel& parsedModel);
pair<bool, AnyExprRef> tryParseExpr(json& essenceExpr, ParsedModel& parsedModel);

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

AnyExprRef parseExpr(json& essenceExpr, ParsedModel& parsedModel) {
    auto boolConstraintPair = tryParseExpr(essenceExpr, parsedModel);
    if (boolConstraintPair.first) {
        return move(boolConstraintPair.second);
    } else {
        cerr << "Failed to parse expression: " << essenceExpr << endl;
        abort();
    }
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

template <typename RetType, typename Constraint, typename Func>
RetType expect(Constraint&& constraint, Func&& func) {
    return mpark::visit(
        [&](auto&& constraint) -> RetType {
            return staticIf<HasReturnType<
                RetType, BaseType<decltype(constraint)>>::value>(
                       [&](auto&& constraint) -> RetType {
                           return std::forward<decltype(constraint)>(
                               constraint);
                       })
                .otherwise([&](auto&& constraint) -> RetType {
                    func(std::forward<decltype(constraint)>(constraint));
                    abort();
                })(std::forward<decltype(constraint)>(constraint));
        },
        std::forward<Constraint>(constraint));
}

shared_ptr<OpIntEq> parseOpIntEq(json& intEqExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected int returning expression within Op Int Eq: ";
    IntReturning left = expect<IntReturning>(
        parseExpr(intEqExpr[0], parsedModel),
        [&](auto&&) { cerr << errorMessage << intEqExpr[0]; });
    IntReturning right = expect<IntReturning>(
        parseExpr(intEqExpr[1], parsedModel),
        [&](auto&&) { cerr << errorMessage << intEqExpr[1]; });
    return make_shared<OpIntEq>(std::move(left), std::move(right));
}

pair<bool, AnyExprRef> tryParseExpr(json& essenceExpr, ParsedModel& parsedModel) {
    if (essenceExpr.count("Op")) {
        auto& op = essenceExpr["Op"];
        if (op.count("MkOpEq")) {
            return std::make_pair(true,
                                  parseOpIntEq(op["MkOpEq"], parsedModel));
        }
    }
    auto boolValuePair = tryParseValue(essenceExpr, parsedModel);
    if (boolValuePair.first) {
        return std::make_pair(
            true,
            mpark::visit([](auto& val) -> AnyExprRef { return move(val); },
                         boolValuePair.second));
    }
    return std::make_pair(false, ValRef<IntValue>(nullptr));
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

void handleFindDeclaration(json& findArray, ParsedModel& parsedModel) {
    string findName = findArray[1]["Name"];
    auto findDomain = parseDomain(findArray[2], parsedModel);
    mpark::visit(
        [&](auto& domainImpl) {
            parsedModel.vars.emplace(
                findName,
                AnyValRef(parsedModel.builder.addVariable(domainImpl)));
        },
        findDomain);
}

void parseExprs(json& suchThat, ParsedModel& parsedModel) {
    for (auto& op : suchThat) {
        BoolReturning constraint =
            expect<BoolReturning>(parseExpr(op, parsedModel), [&](auto&&) {
                cerr << "Expected Bool returning constraint within such that: "
                     << op << endl;
            });
        parsedModel.builder.addConstraint(constraint);
    }
}

void parseModel(const int argc, const char** argv) {
    argParser.validateArgs(argc, argv);
    auto& is = fileArg.get();
    json j;
    is >> j;
    ParsedModel parsedModel;
    for (auto& statement : j["mStatements"]) {
        if (statement.count("Declaration")) {
            auto& declaration = statement["Declaration"];
            if (declaration.count("Letting")) {
                handleLettingDeclaration(declaration["Letting"], parsedModel);
            } else if (declaration.count("FindOrGiven") &&
                       declaration["FindOrGiven"][0] == "Find") {
                handleFindDeclaration(declaration["FindOrGiven"], parsedModel);
            }
        } else if (statement.count("SuchThat")) {
            parseExprs(statement["SuchThat"], parsedModel);
        }
    }
    cout << parsedModel.vars << endl;
    cout << parsedModel.domainLettings << endl;
}
