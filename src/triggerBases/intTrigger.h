
#ifndef SRC_TRIGGERBASES_INTTRIGGER_H_
#define SRC_TRIGGERBASES_INTTRIGGER_H_
#include "types/forwardDecls/typesAndDomains.h"

struct IntTrigger {
    virtual void preValueChange(const IntValue& oldValue) = 0;
    virtual void postValueChange(const IntValue& newValue) = 0;
};

std::vector<std::shared_ptr<IntTrigger>>& getTriggers(IntValue& value);
#endif /* SRC_TRIGGERBASES_INTTRIGGER_H_ */
