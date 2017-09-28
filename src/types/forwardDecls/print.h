
#ifndef SRC_TYPES_FORWARDDECLS_PRINT_H_
#define SRC_TYPES_FORWARDDECLS_PRINT_H_
#define makePrettyPrintDecl(valueName) \
    std::ostream& prettyPrint(std::ostream& os, const valueName##Value&);
#include <iostream>
#include "types/forwardDecls/typesAndDomains.h"
buildForAllTypes(makePrettyPrintDecl, )
#undef makePrettyPrintDecl
#endif
