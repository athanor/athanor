
#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include <utility>
#include <vector>

#include "types/base.h"
struct BoolDomain {};
struct BoolTrigger : public IterAssignedTrigger<BoolValue> {
    typedef BoolView View;
    virtual void possibleValueChange(u_int64_t OldViolation) = 0;
    virtual void valueChanged(u_int64_t newViolation) = 0;
};

struct BoolView {
    u_int64_t violation;
    std::vector<std::shared_ptr<BoolTrigger>> triggers;
};

struct BoolValue : public BoolView, ValBase {
    template <typename Func>
    inline void changeValue(Func&& func) {
        u_int64_t oldViolation = violation;
        func();
        if (violation == oldViolation) {
            return;
        }
        visitTriggers(
            [&](auto& trigger) { trigger->possibleValueChange(oldViolation); },
            triggers);
        visitTriggers([&](auto& trigger) { trigger->valueChanged(violation); },
                      triggers);
    }
};

#endif /* SRC_TYPES_BOOL_H_ */
