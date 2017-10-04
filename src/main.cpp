#include <operators/setProducing.h>
#include <iostream>
#include "common/common.h"
#include "operators/setOperators.h"
#include "types/allTypes.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "utils/hashUtils.h"
using namespace std;

int main() {
    SetDomain s(noSize(), IntDomain({intBound(1, 3)}));
    auto l = construct<SetValue>();
    auto r = construct<SetValue>();
    IntProducing intersectSize =
        make_shared<OpSetSize>(make_shared<OpSetIntersect>(l, r));
    IntView sizeView = getIntView(intersectSize);
    assignInitialValueInDomain(s, *l);
    assignInitialValueInDomain(s, *r);
    do {
        do {
            cout << "l: ";
            prettyPrint(cout, *l) << endl;
            cout << "r: ";
            prettyPrint(cout, *r) << endl;
            cout << "Intersect size: " << sizeView.value << endl;
        } while (moveToNextValueInDomain(*r, s, []() { return true; }));
    } while (moveToNextValueInDomain(*l, s, []() { return true; }));
}
