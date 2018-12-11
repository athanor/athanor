#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include "base/base.h"
#include "triggers/boolTrigger.h"
#include "utils/ignoreUnused.h"

struct BoolView : public ExprInterface<BoolView>,
                  public TriggerContainer<BoolView> {
    UInt violation;

    template <typename Func>
    inline bool changeValue(Func&& func) {
        UInt oldViolation = violation;
        if (func() && violation != oldViolation) {
            visitTriggers([&](auto& trigger) { trigger->valueChanged(); },
                          triggers);
            return true;
        }
        return false;
    }
    void standardSanityChecksForThisType() const;
};


struct BoolViolationContext : public ViolationContext {
    bool negated;
    BoolViolationContext(UInt parentViolation, bool negated)
        : ViolationContext(parentViolation), negated(negated) {}
};

#endif /* SRC_TYPES_BOOL_H_ */
