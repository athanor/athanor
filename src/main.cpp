#include <iostream>
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/opIntEq.h"
#include "operators/opMod.h"
#include "operators/opSetForAll.h"
#include "operators/opSetNotEq.h"
#include "operators/opSetSize.h"
#include "search/searchStrategies.h"
#include "types/allTypes.h"
#include "types/typeOperations.h"

using namespace std;

int main() {
    auto zeroConst = ValRef<IntValue>(make_shared<IntValue>());
    zeroConst->value = 0;
    auto twoConst = ValRef<IntValue>(make_shared<IntValue>());
    twoConst->value = 2;
    auto _20Const = ValRef<IntValue>(make_shared<IntValue>());
    _20Const->value = 10;

    ModelBuilder builder;
    auto domain =
        make_shared<SetDomain>(noSize(), IntDomain({intBound(1, 20)}));
    auto a = builder.addVariable(domain);
    auto forAll = make_shared<OpSetForAll>(a);
    auto i = forAll->newIterRef<IntValue>();
    forAll->setExpression(
        make_shared<OpIntEq>(make_shared<OpMod>(i, twoConst), zeroConst));
    builder.addConstraint(forAll);
    builder.addConstraint(
        make_shared<OpIntEq>(make_shared<OpSetSize>(a), _20Const));
    HillClimber<InteractiveNeighbourhoodSelector> search(builder.build());
    search.search();
}
