#include <iostream>
#include "common/common.h"
#include "constructorShortcuts.h"
#include "search/searchStrategies.h"
#include "types/typeOperations.h"

using namespace std;

int main() {
    auto zeroConst = ValRef<IntValue>(make_shared<IntValue>());
    zeroConst->value = 0;
    auto modulousConst = ValRef<IntValue>(make_shared<IntValue>());
    modulousConst->value = 2;
    auto sizeLimitConst = ValRef<IntValue>(make_shared<IntValue>());
    sizeLimitConst->value = 10;

    ModelBuilder builder;
    auto domain = make_shared<SetDomain>(
        maxSize(5), SetDomain(noSize(), IntDomain({intBound(1, 10)})));
    auto a = builder.addVariable(domain);
    auto forAll = setForAll(a);
    auto i = forAll->newIterRef<SetValue>();
    auto innerForAll = setForAll(i);
    forAll->setExpression(innerForAll);
    auto j = innerForAll->newIterRef<IntValue>();
    innerForAll->setExpression(opAnd(intEq(setSize(i), sizeLimitConst),
                                     intEq(mod(j, modulousConst), zeroConst)));
    builder.addConstraint(forAll);

    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}
