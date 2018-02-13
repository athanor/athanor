
#include <autoArgParse/argParser.h>
#include <iostream>
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
int main(const int argc, const char** argv) { sonet(argc, argv); }

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
    auto outerSum = opSum(outerIntQuant);
    auto k = outerIntQuant->newIterRef<SetValue>();
    outerIntQuant->setExpression(setSize(k));
    builder.setObjective(OptimiseMode::MAXIMISE, outerSum);
    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}

void sonet(const int argc, const char** argv) {
    using namespace AutoArgParse;
    ArgParser argParser;
    auto& fileArg = argParser.add<Arg<ifstream>>(
        "path_to_file", Policy::MANDATORY, "Path to sonet instance.",
        [](const std::string& path) -> ifstream {
            ifstream file;
            file.open(path);
            if (file.good()) {
                return file;
            } else {
                throw ErrorMessage("Error opening file: " + path);
            }
        });
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
    ModelBuilder builder;

    auto mSetDomain = make_shared<MSetDomain>(
        exactSize(numberRings), SetDomain(maxSize(capacity), nodesDomain));
    auto networkVar = builder.addVariable(mSetDomain);
    auto outerIntQuant = quant<IntReturning>(networkVar);
    auto outerSum = opSum(outerIntQuant);
    auto k = outerIntQuant->newIterRef<SetValue>();
    outerIntQuant->setExpression(setSize(k));
    builder.setObjective(OptimiseMode::MINIMISE, outerSum);
    //--tbc
    todoImpl();
    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}
