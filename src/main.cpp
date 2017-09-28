#include <iostream>
#include "types/allTypes.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/print.h"

int main() {
    SetDomain domain;
    domain.sizeAttr = minSize(5);
    auto intDom = std::make_shared<IntDomain>();
    intDom->bounds = {std::make_pair(1, 3), std::make_pair(5, 8)};
    domain.inner = std::move(intDom);
    auto v = makeInitialValueInDomain(domain);
    prettyPrint(std::cout, *v) << std::endl;
}
