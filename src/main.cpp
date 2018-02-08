
#include <iostream>
#include "common/common.h"
#include "constructorShortcuts.h"
#include "search/searchStrategies.h"
#include "types/typeOperations.h"

using namespace std;
void setOfSetWithModulous();
void setWithModulous();

int main() { setWithModulous(); }

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
    sizeLimitConst->value = 10;
    ModelBuilder builder;

    auto domain =
        make_shared<SetDomain>(minSize(1), IntDomain({intBound(1, 40)}));
    auto a = builder.addVariable(domain);
    builder.addConstraint(intEq(setSize(a), sizeLimitConst));

    auto outerQuant = quant<BoolReturning>(a);
    auto outerForall = opAnd(outerQuant);
    auto i = outerQuant->newIterRef<IntValue>();
    outerQuant->setExpression(intEq(mod(i, modulousConst), zeroConst));
    builder.addConstraint(outerForall);
    HillClimber<InteractiveNeighbourhoodSelector> search(builder.build());
    search.search();
}
