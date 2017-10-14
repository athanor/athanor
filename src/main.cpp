#include <operators/setProducing.h>
#include <iostream>
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/setOperators.h"
#include "search/searchStrategies.h"
#include "types/allTypes.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "utils/hashUtils.h"
using namespace std;

int main() {
    ModelBuilder builder;
    auto domain = make_shared<SetDomain>(noSize(), IntDomain({intBound(1, 4)}));
    auto a = builder.addVariable(domain);
    auto b = builder.addVariable(domain);
    builder.setObjective(
        OptimiseMode::MAXIMISE,
        make_shared<OpSetSize>(make_shared<OpSetIntersect>(a, b)));
    HillClimber<RandomNeighbourhoodSelection> search(builder.build());
    search.search();
}
