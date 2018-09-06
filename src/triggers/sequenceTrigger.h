
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

struct SequenceTrigger : public virtual SequenceOuterTrigger,
                         public virtual SequenceMemberTrigger {};
template <typename Child>
struct ChangeTriggerAdapter<SequenceTrigger, Child>
    : public ChangeTriggerAdapterBase<SequenceTrigger, Child> {
    inline void valueRemoved(UInt, const AnyExprRef&) {
        this->forwardValueChanged();
    }
    inline void valueAdded(UInt, const AnyExprRef&) final {
        this->forwardValueChanged();
    }

    inline void subsequenceChanged(UInt, UInt) final {
        this->forwardValueChanged();
        ;
    }
    inline void positionsSwapped(UInt, UInt) final {
        this->forwardValueChanged();
    }
    inline void memberHasBecomeDefined(UInt) {}
    inline void memberHasBecomeUndefined(UInt) {}
};

#endif /* SRC_TRIGGERS_SEQUENCETRIGGER_H_ */
