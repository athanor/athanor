
#ifndef SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#define SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#include "types/forwardDecls/typesAndDomains.h"
#define assignRandomFunctions(name)                            \
    void assignRandomValueInDomain(const name##Domain& domain, \
                                   name##Value& val);
buildForAllTypes(assignRandomFunctions, )
#undef assignRandomFunctions

#endif /* SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_ */
