#ifndef SRC_OPERATORS_INTPRODUCING_H_
#define SRC_OPERATORS_INTPRODUCING_H_
#include "types/forwardDecls/typesAndDomains.h"
struct OpSetSize;

struct IntTrigger {
    virtual void possibleValueChange(int64_t value) = 0;
    virtual void valueChanged(int64_t value) = 0;
};

using IntProducing =
    mpark::variant<ValRef<IntValue>, std::shared_ptr<OpSetSize>>;

struct IntView {
    int64_t& value;
    std::vector<std::shared_ptr<IntTrigger>>& triggers;
    IntView(int64_t& value, std::vector<std::shared_ptr<IntTrigger>>& triggers)
        : value(value), triggers(triggers) {}
};
IntView getIntView(IntValue& value);
IntView getIntView(OpSetSize& op);
inline IntView getIntView(IntProducing& val) {
    return mpark::visit([&](auto& valImpl) { return getIntView(*valImpl); },
                        val);
}

#endif /* SRC_OPERATORS_INTPRODUCING_H_ */
