#ifndef SRC_OPERATORS_BOOLPRODUCING_H_
#define SRC_OPERATORS_BOOLPRODUCING_H_
#include "types/forwardDecls/typesAndDomains.h"
struct OpSetEq;

struct BoolTrigger {
    virtual void preValueChange(const BoolValue& oldValue) = 0;
    virtual void postValueChange(const BoolValue& newValue) = 0;
};

using BoolProducing =
    mpark::variant<std::shared_ptr<BoolValue>, std::shared_ptr<OpSetEq>>;

struct BoolView {
    std::vector<std::shared_ptr<BoolTrigger>>& triggers;
    BoolView(std::vector<std::shared_ptr<BoolTrigger>>& triggers)
        : triggers(triggers) {}
};

BoolView getBooView(BoolValue& value);
BoolView getBoolView(OpSetEq& op);
#endif /* SRC_OPERATORS_BOOLPRODUCING_H_ */
