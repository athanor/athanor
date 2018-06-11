#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include <utility>
#include <vector>
#include "utils/ignoreUnused.h"

#include "base/base.h"
struct BoolDomain {};
struct BoolView : public ExprInterface<BoolView> {
    UInt violation;
    std::vector<std::shared_ptr<BoolTrigger>> triggers;

    inline void initFrom(BoolView& other) { violation = other.violation; }
    template <typename Func>
    inline bool changeValue(Func&& func) {
        UInt oldViolation = violation;
        if (func() && violation != oldViolation) {
            std::swap(oldViolation, violation);
            visitTriggers(
                [&](auto& trigger) { trigger->possibleValueChange(); },
                triggers);
            std::swap(oldViolation, violation);
            visitTriggers([&](auto& trigger) { trigger->valueChanged(); },
                          triggers);
            return true;
        }
        return false;
    }
};

struct BoolValue : public BoolView, ValBase {
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer&) final;
    ExprRef<BoolView> deepCopySelfForUnrollImpl(
        const ExprRef<BoolView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;

    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    bool optimise(PathExtension) final;
};

template <typename Child>
struct ChangeTriggerAdapter<BoolTrigger, Child>
    : public ChangeTriggerAdapterBase<BoolTrigger, Child> {};

#endif /* SRC_TYPES_BOOL_H_ */
