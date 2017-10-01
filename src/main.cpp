#include <iostream>
#include "common/common.h"
#include "types/allTypes.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "utils/hashUtils.h"
using namespace std;

int main() {
    SetDomain s(noSize(), SetDomain(noSize(), IntDomain({intBound(1, 3)})));
    cout << "Building values with domain: ";
    prettyPrint(cout, s) << endl;
    auto v = makeInitialValueInDomain(s);

    do {
        prettyPrint(cout, *v) << endl;
    } while (moveToNextValueInDomain(*v, s, []() { return true; }));
}
