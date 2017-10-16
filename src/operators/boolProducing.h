#ifndef SRC_OPERATORS_BOOLPRODUCING_H_
#define SRC_OPERATORS_BOOLPRODUCING_H_
#include "types/forwardDecls/typesAndDomains.h"
#define buildForBoolProducers(f, sep) \
    f(BoolValue) sep f(OpAnd) sep f(OpSetNotEq)

#define structDecls(name) struct name;
buildForBoolProducers(structDecls, )
#undef structDecls

    struct BoolTrigger {
    virtual void possibleValueChange(u_int64_t OldViolation) = 0;
    virtual void valueChanged(u_int64_t newViolation) = 0;
};

using BoolProducing =
    mpark::variant<std::shared_ptr<BoolValue>, std::shared_ptr<OpAnd>,
                   std::shared_ptr<OpSetNotEq>>;

struct BoolView {
    u_int64_t& violation;
    std::vector<std::shared_ptr<BoolTrigger>>& triggers;
    BoolView(u_int64_t& violation,
             std::vector<std::shared_ptr<BoolTrigger>>& triggers)
        : violation(violation), triggers(triggers) {}
};

#define boolProducerFuncs(name)  \
    BoolView getBoolView(name&); \
    void evaluate(name&);        \
    void startTriggering(name&); \
    void stopTriggering(name&);
buildForBoolProducers(boolProducerFuncs, )
#undef boolProducerFuncs
    inline BoolView getBoolView(BoolProducing& boolV) {
    return mpark::visit([&](auto& boolImpl) { return getBoolView(*boolImpl); },
                        boolV);
}

inline void evaluate(BoolProducing& boolV) {
    mpark::visit([&](auto& boolImpl) { evaluate(*boolImpl); }, boolV);
}

inline void startTriggering(BoolProducing& boolV) {
    mpark::visit([&](auto& boolImpl) { startTriggering(*boolImpl); }, boolV);
}

inline void stopTriggering(BoolProducing& boolV) {
    mpark::visit([&](auto& boolImpl) { stopTriggering(*boolImpl); }, boolV);
}

#endif /* SRC_OPERATORS_BOOLPRODUCING_H_ */
