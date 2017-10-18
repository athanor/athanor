#ifndef SRC_OPERATORS_INTPRODUCING_H_
#define SRC_OPERATORS_INTPRODUCING_H_
#include "search/violationDescription.h"
#include "types/forwardDecls/typesAndDomains.h"

#define buildForIntProducers(f, sep) f(IntValue) sep f(OpSetSize) sep f(OpSum)

#define structDecls(name) struct name;
buildForIntProducers(structDecls, )
#undef structDecls

    struct IntTrigger {
    virtual void possibleValueChange(int64_t value) = 0;
    virtual void valueChanged(int64_t value) = 0;
};

using IntProducing =
    mpark::variant<ValRef<IntValue>, std::shared_ptr<OpSetSize>,
                   std::shared_ptr<OpSum>>;

struct IntView {
    int64_t& value;
    std::vector<std::shared_ptr<IntTrigger>>& triggers;
    IntView(int64_t& value, std::vector<std::shared_ptr<IntTrigger>>& triggers)
        : value(value), triggers(triggers) {}
};

#define intProducerFuncs(name)                                                 \
    IntView getIntView(name&);                                                 \
    void evaluate(name&);                                                      \
    void startTriggering(name&);                                               \
    void stopTriggering(name&);                                                \
    void updateViolationDescription(const name& op, u_int64_t parentViolation, \
                                    ViolationDescription&);
buildForIntProducers(intProducerFuncs, )
#undef intProducerFuncs
    inline IntView getIntView(IntProducing& intV) {
    return mpark::visit([&](auto& intImpl) { return getIntView(*intImpl); },
                        intV);
}

inline void evaluate(IntProducing& intV) {
    mpark::visit([&](auto& intImpl) { evaluate(*intImpl); }, intV);
}

inline void startTriggering(IntProducing& intV) {
    mpark::visit([&](auto& intImpl) { startTriggering(*intImpl); }, intV);
}
inline void stopTriggering(IntProducing& intV) {
    mpark::visit([&](auto& intImpl) { stopTriggering(*intImpl); }, intV);
}
inline void updateViolationDescription(
    const IntProducing& intV, u_int64_t parentViolation,
    ViolationDescription& violationDescription) {
    mpark::visit(
        [&](auto& boolImpl) {
            updateViolationDescription(*boolImpl, parentViolation,
                                       violationDescription);
        },
        intV);
}

#endif /* SRC_OPERATORS_INTPRODUCING_H_ */
