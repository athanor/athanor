#include <iostream>
#include "common/common.h"
#include "constructorShortcuts.h"
#include "search/searchStrategies.h"
#include "types/typeOperations.h"

using namespace std;
void setOfSetWithModulous();
int main() {
setOfSetWithModulous();
}

void setOfSetWithModulous() {
    auto zeroConst = ValRef<IntValue>(make_shared<IntValue>());
    zeroConst->value = 0;
    auto modulousConst = ValRef<IntValue>(make_shared<IntValue>());
    modulousConst->value = 2;
    auto sizeLimitConst = ValRef<IntValue>(make_shared<IntValue>());
    sizeLimitConst->value = 3;

    ModelBuilder builder;
    auto domain = make_shared<SetDomain>(
        maxSize(3), SetDomain(noSize(), IntDomain({intBound(1, 8)})));
    auto a = builder.addVariable(domain);
    auto forAll = setForAll(a);
    auto i = forAll->newIterRef<SetValue>();
    auto innerForAll = setForAll(i);
    forAll->setExpression(
        opAnd(intEq(setSize(i), sizeLimitConst), innerForAll));
    auto j = innerForAll->newIterRef<IntValue>();
    innerForAll->setExpression(intEq(mod(j, modulousConst), zeroConst));
    builder.addConstraint(forAll);
    builder.addConstraint(intEq(setSize(a), sizeLimitConst));
    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}
