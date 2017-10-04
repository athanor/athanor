#ifndef SRC_OPERATORS_BOOLPRODUCING_H_
#define SRC_OPERATORS_BOOLPRODUCING_H_
#include "types/forwardDecls/typesAndDomains.h"
struct OpIntEq;

struct BoolTrigger {
    virtual void postValueChange() = 0;
};

using BoolProducing =
    mpark::variant<std::shared_ptr<BoolValue>, std::shared_ptr<OpIntEq>>;

struct BoolView {
    u_int64_t& violation;
    std::vector<std::shared_ptr<BoolTrigger>>& triggers;
    BoolView(u_int64_t& violation,
             std::vector<std::shared_ptr<BoolTrigger>>& triggers)
        : violation(violation), triggers(triggers) {}
};

BoolView getBoolView(BoolValue& value);
BoolView getBoolView(OpIntEq& op);

inline BoolView getBoolView(BoolProducing& val) {
    return mpark::visit([&](auto& valImpl) { return getBoolView(*valImpl); },
                        val);
}

#endif /* SRC_OPERATORS_BOOLPRODUCING_H_ */
