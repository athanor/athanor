
#ifndef SRC_TRIGGERS_SEQUENCETRIGGER_H_
#define SRC_TRIGGERS_SEQUENCETRIGGER_H_
#include "base/base.h"
struct SequenceOuterTrigger : public virtual TriggerBase {
    virtual void valueRemoved(
        UInt index, const AnyExprRef& removedValueindexOfRemovedValue) = 0;
    virtual void valueAdded(UInt indexOfRemovedValue,
                            const AnyExprRef& member) = 0;
    virtual void memberHasBecomeUndefined(UInt) = 0;
    virtual void memberHasBecomeDefined(UInt) = 0;
};

struct SequenceMemberTrigger : public virtual TriggerBase {
    virtual void subsequenceChanged(UInt startIndex, UInt endIndex) = 0;
    virtual void positionsSwapped(UInt index1, UInt index2) = 0;
};

/* don't want to import SequenceView here but need access to internal field so a
 * function has been created for that purpose*/

struct SequenceTrigger : public virtual SequenceOuterTrigger,
                         public virtual SequenceMemberTrigger {};

template <typename Child>
struct ChangeTriggerAdapter<SequenceTrigger, Child>
    : public ChangeTriggerAdapterBase<SequenceTrigger, Child> {
   private:
    bool wasDefined;
    inline bool eventHandledAsDefinednessChange() {
        bool defined = getOp()->getViewIfDefined().hasValue();
        if (wasDefined && !defined) {
            this->forwardHasBecomeUndefined();
            wasDefined = defined;
            return true;
        } else if (!wasDefined && defined) {
            this->forwardHasBecomeDefined();
            wasDefined = defined;
            return true;
        } else {
            wasDefined = defined;
            return !defined;
        }
    }

    inline auto& getOp() {
        return static_cast<Child*>(this)->getTriggeringOperand();
    }

   public:
    ChangeTriggerAdapter(const ExprRef<SequenceView>& op)
        : wasDefined(op->getViewIfDefined().hasValue()) {}
    inline void valueRemoved(UInt, const AnyExprRef&) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    inline void valueAdded(UInt, const AnyExprRef&) final {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }

    inline void subsequenceChanged(UInt, UInt) final {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    inline void positionsSwapped(UInt, UInt) final {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    inline void memberHasBecomeDefined(UInt) {
        eventHandledAsDefinednessChange();
    }
    inline void memberHasBecomeUndefined(UInt) {
        eventHandledAsDefinednessChange();
    }
    inline void hasBecomeDefined() { eventHandledAsDefinednessChange(); }
    inline void hasBecomeUndefined() { eventHandledAsDefinednessChange(); }
};

#endif /* SRC_TRIGGERS_SEQUENCETRIGGER_H_ */
