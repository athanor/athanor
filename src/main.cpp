#include <iostream>
#include "types/allTypes.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/print.h"
using namespace std;
#define EInt(...)                \
    std::make_shared<IntDomain>( \
        std::vector<std::pair<u_int64_t, int64_t>>({__VA_ARGS__}))
int main() {
    SetDomain s(exactSize(8), SetDomain(noSize(), IntDomain({intBound(1, 3)})));
    cout << "Building values with domain: ";
    prettyPrint(cout, s) << endl;
    auto v = makeInitialValueInDomain(s);
    do {
        prettyPrint(cout, *v) << endl;
    } while (moveToNextValueInDomain(*v, s, []() { return true; }));
}
