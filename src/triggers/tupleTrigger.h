
#ifndef SRC_TRIGGERS_TUPLETRIGGER_H_
#define SRC_TRIGGERS_TUPLETRIGGER_H_
#include "base/base.h"
struct TupleTrigger : public virtual TriggerBase {
    virtual void memberValueChanged(UInt index) = 0;
    virtual void memberHasBecomeUndefined(UInt) = 0;
    virtual void memberHasBecomeDefined(UInt) = 0;
};

/* don't want to import tuple view here but need access to internal field so a
 * function has been created for that purpose*/
size_t numberUndefinedMembers(const TupleView& view);

template <typename Child>
struct ChangeTriggerAdapter<TupleTrigger, Child>
    : public ChangeTriggerAdapterBase<TupleTrigger, Child> {
    inline void memberValueChanged(UInt) final { this->forwardValueChanged(); }
    inline void memberHasBecomeDefined(UInt) {
        ExprRef<TupleView> tuple =
            static_cast<Child*>(this)->getTriggeringTuple();
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
            static_cast<Child*>(this)->getTriggeringTuple();
        auto view = tuple->view();
        if (view && numberUndefinedMembers(view.get()) == 1) {
            this->forwardHasBecomeUndefined();
        }
    }
};

#endif /* SRC_TRIGGERS_TUPLETRIGGER_H_ */
