#ifndef SRC_OPERATORS_OPERATORS_H_
#define SRC_OPERATORS_OPERATORS_H_
#include "types/forwardDecls/typesAndDomains.h"

#define buildForAllOps(f, sep) \
    f(OpSetIntersect) sep f(OpSetUnion) sep f(OpSetSize) sep f(OpSetEq)

#define forwardDeclOps(op) struct op;
buildForAllOps(forwardDeclOps, )
#undef forwardDeclOps

    // group operators by the type they evaluate to

    using BoolProducing =
        mpark::variant<std::shared_ptr<BoolValue>, std::shared_ptr<OpSetEq>>;
using IntProducing =
    mpark::variant<std::shared_ptr<IntValue>, std::shared_ptr<OpSetSize>>;
using SetProducing =
    mpark::variant<std::shared_ptr<SetValue>, std::shared_ptr<OpSetIntersect>,
                   std::shared_ptr<OpSetUnion>>;
#endif /* SRC_OPERATORS_OPERATORS_H_ */
