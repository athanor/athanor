
#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include <utility>
#include <vector>

#include "forwardDecls/typesAndDomains.h"
#include "operators/boolProducing.h"
struct BoolDomain {};
struct BoolValue {
    bool value;
    std::vector<std::shared_ptr<BoolTrigger>> triggers;

    template <typename Func>
    inline void changeValue(Func&& func) {
        for (auto& trigger : triggers) {
            trigger->possibleValueChange(value);
        }
        func();
        for (auto& trigger : triggers) {
            trigger->valueChanged(value);
        }
    }
};

#endif /* SRC_TYPES_BOOL_H_ */
