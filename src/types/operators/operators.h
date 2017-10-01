#ifndef SRC_TYPES_OPERATORS_OPERATORS_H_
#define SRC_TYPES_OPERATORS_OPERATORS_H_
#include "types/allTypesAndDomains.h"

#define buildForAllOps(f, sep) \
    f(OpSetIntersect) sep f(OpSetUnion) sep f(OpSetSize) sep f(OpSetEq)

#define forwardDeclOps(op) struct op;
buildForAllOps(forwardDeclOps, )
#undef forwardDeclOps

    // group operators by the type they evaluate to
    using SetProducing = mpark::variant<std::shared_ptr<SetValue>,
                                        std::shared_ptr<OpSetIntersect>,
                                        std::shared_ptr<OpSetUnion>>
#endif /* SRC_TYPES_OPERATORS_OPERATORS_H_ */
