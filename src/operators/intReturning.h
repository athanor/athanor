#ifndef SRC_OPERATORS_INTRETURNING_H_
#define SRC_OPERATORS_INTRETURNING_H_
#include "operators/quantifierBase.h"
#include "search/violationDescription.h"
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

    inline IntView& getIntView(const IntReturning& intV) {
    return mpark::visit(
        [&](auto& intImpl) -> IntView& {
            return reinterpret_cast<IntView&>(*intImpl);
        },
        intV);
}

inline void evaluate(IntReturning& intV) {
    mpark::visit([&](auto& intImpl) { evaluate(*intImpl); }, intV);
}

inline void startTriggering(IntReturning& intV) {
    mpark::visit([&](auto& intImpl) { startTriggering(*intImpl); }, intV);
}
inline void stopTriggering(IntReturning& intV) {
    mpark::visit([&](auto& intImpl) { stopTriggering(*intImpl); }, intV);
}
inline void updateViolationDescription(
    const IntReturning& intV, u_int64_t parentViolation,
    ViolationDescription& violationDescription) {
    mpark::visit(
        [&](auto& boolImpl) {
            updateViolationDescription(*boolImpl, parentViolation,
                                       violationDescription);
        },
        intV);
}

#endif /* SRC_OPERATORS_INTRETURNING_H_ */
