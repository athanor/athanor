#include <iostream>
#include "types/allTypes.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/print.h"
using namespace std;

int main() {
    SetDomain s(exactSize(4),
                SetDomain(maxSize(1), IntDomain({intBound(1, 3)})));
    cout << "Building values with domain: ";
    prettyPrint(cout, s) << endl;
    auto v = makeInitialValueInDomain(s);
    do {
        prettyPrint(cout, *v) << endl;
    } while (moveToNextValueInDomain(*v, s, []() { return true; }));
}
