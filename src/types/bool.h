
#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include <utility>
#include <vector>
#include "utils/ignoreUnused.h"

#include "base/base.h"
struct BoolDomain {};
struct BoolTrigger : public IterAssignedTrigger<BoolView> {
    virtual void possibleValueChange(UInt OldViolation) = 0;
    virtual void valueChanged(UInt newViolation) = 0;
};

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
                [&](auto& trigger) { trigger->possibleValueChange(violation); },
                triggers);
            std::swap(oldViolation, violation);
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
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
};

template <>
struct DefinedTrigger<BoolValue> : public BoolTrigger {
    ValRef<BoolValue> val;
    DefinedTrigger(const ValRef<BoolValue>& val) : val(val) {}
    inline void possibleValueChange(UInt) final {}
    inline void valueChanged(UInt violation) {
        val->changeValue([&]() {
            val->violation = violation;
            return true;
        });
    }
    inline void preIterValueChange(const ExprRef<BoolView>&) final {
        todoImpl();
    }
    inline void postIterValueChange(const ExprRef<BoolView>&) final {
        assert(false);
        abort();
    }
};
template <typename Child>
struct ChangeTriggerAdapter<BoolTrigger, Child>
    : public BoolTrigger, public ChangeTriggerAdapterBase<Child> {
    inline void possibleValueChange(UInt) final {
        this->forwardPossibleValueChange();
    }
    inline void valueChanged(UInt) final { this->forwardValueChanged(); }
    inline void preIterValueChange(const ExprRef<BoolView>&) final {
        this->forwardPossibleValueChange();
    }
    inline void postIterValueChange(const ExprRef<BoolView>&) final {
        this->forwardValueChanged();
    }
};

#endif /* SRC_TYPES_BOOL_H_ */
