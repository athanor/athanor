
#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include <utility>
#include <vector>

#include "base/base.h"
struct BoolDomain {};
struct BoolTrigger : public IterAssignedTrigger<BoolView> {
    virtual void possibleValueChange(u_int64_t OldViolation) = 0;
    virtual void valueChanged(u_int64_t newViolation) = 0;
};

struct BoolView : public ExprInterface<BoolView> {
    u_int64_t violation;
    std::vector<std::shared_ptr<BoolTrigger>> triggers;

    inline void initFrom(BoolView& other) { violation = other.violation; }
    template <typename Func>
    inline bool changeValue(Func&& func) {
        u_int64_t oldViolation = violation;
        if (func() && violation != oldViolation) {
            visitTriggers(
                [&](auto& trigger) {
                    trigger->possibleValueChange(oldViolation);
                },
                triggers);
            visitTriggers(
                [&](auto& trigger) { trigger->valueChanged(violation); },
                triggers);
            return true;
        }
        return false;
    }
};

struct BoolValue : public BoolView, ValBase {
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(u_int64_t parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopyForUnroll(const ExprRef<BoolView>& op,
                                        const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
};

template <>
struct DefinedTrigger<BoolValue> : public BoolTrigger {
    ValRef<BoolValue> val;
    DefinedTrigger(const ValRef<BoolValue>& val) : val(val) {}
    inline void possibleValueChange(u_int64_t) final {}
    inline void valueChanged(u_int64_t violation) {
        val->changeValue([&]() {
            val->violation = violation;
            return true;
        });
    }
    void iterHasNewValue(const BoolView&, const ExprRef<BoolView>&) final {
        assert(false);
        abort();
    }
};

#endif /* SRC_TYPES_BOOL_H_ */
