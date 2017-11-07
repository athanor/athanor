#ifndef SRC_OPERATORS_SETRETURNING_H_
#define SRC_OPERATORS_SETRETURNING_H_
#include <unordered_set>
#include <vector>
#include "operators/operatorBase.h"
#include "types/forwardDecls/typesAndDomains.h"
#include "types/set.h"
// helper macros
#define buildForSetProducers(f, sep) \
    f(SetValue) sep f(OpSetIntersect) sep f(OpSetUnion) sep

#define structDecls(name) struct name;
buildForSetProducers(structDecls, );
#undef structDecls

using SetReturning = mpark::variant<ValRef<SetValue>, QuantRef<SetValue>,
                                    std::shared_ptr<OpSetIntersect>>;

#define setProducerFuncs(name)                                                 \
    void evaluate(name&);                                                      \
    void startTriggering(name&);                                               \
    void stopTriggering(name&);                                                \
    void updateViolationDescription(const name& op, u_int64_t parentViolation, \
                                    ViolationDescription&);
buildForSetProducers(setProducerFuncs, )
#undef setProducerFuncs
#endif /* SRC_OPERATORS_SETRETURNING_H_ */
