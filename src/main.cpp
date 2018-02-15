
#include <autoArgParse/argParser.h>
#include <iostream>
#include <unordered_map>
#include "common/common.h"

#include <fstream>
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
    [](const std::string& path) -> ifstream {
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
void jsonTest();
int main(const int argc, const char** argv) {
    jsonTest();
    //sonet(argc, argv);
    ignoreUnused(argc,argv);
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
