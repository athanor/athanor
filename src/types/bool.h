
#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include <utility>
#include <vector>

#include "forwardDecls/typesAndDomains.h"
struct BoolDomain {};
struct BoolValue : DirtyFlag {
    bool value;
};

#endif /* SRC_TYPES_BOOL_H_ */
