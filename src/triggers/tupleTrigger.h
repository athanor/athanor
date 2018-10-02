
#ifndef SRC_TRIGGERS_TUPLETRIGGER_H_
#define SRC_TRIGGERS_TUPLETRIGGER_H_
#include "base/base.h"
struct TupleOuterTrigger : public virtual TriggerBase {};

struct TupleMemberTrigger : public virtual TriggerBase {
    virtual void memberValueChanged(UInt index) = 0;

    virtual void memberHasBecomeDefined(UInt index) = 0;
    virtual void memberHasBecomeUndefined(UInt index) = 0;
};

struct TupleTrigger : public virtual TupleOuterTrigger,
                      public virtual TupleMemberTrigger {};

size_t numberUndefinedMembers(const TupleView& view);

template <typename Child>
struct ChangeTriggerAdapter<TupleTrigger, Child>
    : public ChangeTriggerAdapterBase<TupleTrigger, Child> {
    inline void memberValueChanged(UInt) final { this->forwardValueChanged(); }
    inline void memberHasBecomeDefined(UInt) {
        ExprRef<TupleView> tuple =
            static_cast<Child*>(this)->getTriggeringOperand();
        auto view = tuple->view();
        if (!view) {
            return;
        }
        if (numberUndefinedMembers(view.get()) == 0) {
            this->forwardHasBecomeDefined();
        }
    }
    inline void memberHasBecomeUndefined(UInt) {
        ExprRef<TupleView> tuple =
            static_cast<Child*>(this)->getTriggeringOperand();
        auto view = tuple->view();
        if (view && numberUndefinedMembers(view.get()) == 1) {
            this->forwardHasBecomeUndefined();
        }
    }
};
#endif /* SRC_TRIGGERS_TUPLETRIGGER_H_ */
