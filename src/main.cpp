#include <operators/setProducing.h>
#include <iostream>
#include "common/common.h"
#include "neighbourhoods/neighbourhoods.h"
#include "operators/setOperators.h"
#include "types/allTypes.h"
#include "types/forwardDecls/constructValue.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "utils/hashUtils.h"
using namespace std;

int main() {
    SetDomain s(noSize(), IntDomain({intBound(1, 5)}));
    auto val = construct<SetValue>();
    matchInnerType(s, *val);
    for (int i = 0; i < 5; ++i) {
        assignRandomValueInDomain(s, *val);
        prettyPrint(cout, val) << endl;
    }
}
