
#ifndef SRC_TRIGGERBASES_SETTRIGGER_H_
#define SRC_TRIGGERBASES_SETTRIGGER_H_
#include "types/forwardDecls/typesAndDomains.h"
struct SetTrigger {
    virtual void valueRemoved(const SetValue& container,
                              const Value& member) = 0;
    virtual void valueAdded(const SetValue& container, const Value& member) = 0;
    virtual void preValueChange(const SetValue& container,
                                const Value& member) = 0;
    virtual void postValueChange(const SetValue& container,
                                 const Value& member) = 0;
};

std::vector<std::shared_ptr<SetTrigger>>& getTriggers(SetValue& value);

#endif /* SRC_TRIGGERBASES_SETTRIGGER_H_ */
