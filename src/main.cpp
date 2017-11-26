#include <iostream>
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/opSetNotEq.h"
#include "search/searchStrategies.h"
#include "types/allTypes.h"
#include "types/typeOperations.h"

using namespace std;

int main() {
    int n = 5;
    ModelBuilder builder;
    auto domain = make_shared<SetDomain>(noSize(), IntDomain({intBound(1, n)}));
    vector<ValRef<SetValue>> vals;
    for (int i = 0; i < (1 << n); ++i) {
        vals.push_back(builder.addVariable(domain));
    }
    for (int i = 0; i < (int)vals.size(); ++i) {
        for (int j = i + 1; j < (int)vals.size(); ++j) {
            builder.addConstraint(make_shared<OpSetNotEq>(vals[i], vals[j]));
        }
    }
    HillClimber<RandomNeighbourhoodWithViolation> search(builder.build());
    search.search();
}
