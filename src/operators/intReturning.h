#ifndef SRC_OPERATORS_INTRETURNING_H_
#define SRC_OPERATORS_INTRETURNING_H_
#include "operators/operatorBase.h"
#include "operators/quantifierBase.h"
#include "types/forwardDecls/typesAndDomains.h"
#include "types/int.h"

#define buildForIntProducers(f, sep) f(IntValue) sep f(OpSetSize) sep f(OpSum)

#define structDecls(name) struct name;
buildForIntProducers(structDecls, );
#undef structDecls

using IntReturning =
    mpark::variant<ValRef<IntValue>, QuantRef<IntValue>,
                   std::shared_ptr<OpSetSize>, std::shared_ptr<OpSum>>;

#define intProducerFuncs(name)                                                 \
    void evaluate(name&);                                                      \
    void startTriggering(name&);                                               \
    void stopTriggering(name&);                                                \
    void updateViolationDescription(const name& op, u_int64_t parentViolation, \
                                    ViolationDescription&);
buildForIntProducers(intProducerFuncs, )
#undef intProducerFuncs

#endif /* SRC_OPERATORS_INTRETURNING_H_ */
