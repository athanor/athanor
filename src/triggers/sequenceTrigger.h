
#ifndef SRC_TRIGGERS_SEQUENCETRIGGER_H_
#define SRC_TRIGGERS_SEQUENCETRIGGER_H_
#include "base/base.h"
struct SequenceOuterTrigger : public virtual TriggerBase {
    virtual void valueRemoved(UInt index, const AnyExprRef& removedValue) = 0;
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

struct TriggerContainer<SequenceView> {
    std::vector<std::shared_ptr<SequenceOuterTrigger>> triggers;
    std::vector<std::shared_ptr<SequenceMemberTrigger>> allMemberTriggers;
    std::vector<std::vector<std::shared_ptr<SequenceMemberTrigger>>>
        singleMemberTriggers;

    template <typename Func>
    void visitAllMemberTriggersInRange(Func&& func, size_t startIndex,
                                       size_t endIndex) {
        visitTriggers(func, allMemberTriggers);
        for (size_t i = startIndex; i < endIndex; i++) {
            if (i >= singleMemberTriggers.size()) {
                break;
            }
            visitTriggers(func, singleMemberTriggers[i]);
        }
    }
};

template <typename Child>
struct ChangeTriggerAdapter<SequenceTrigger, Child>
    : public ChangeTriggerAdapterBase<SequenceTrigger, Child> {
   private:
    bool wasDefined;
    bool eventHandledAsDefinednessChange() {
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

    auto& getOp() { return static_cast<Child*>(this)->getTriggeringOperand(); }

   public:
    ChangeTriggerAdapter(const ExprRef<SequenceView>& op)
        : wasDefined(op->getViewIfDefined().hasValue()) {}
    void valueRemoved(UInt, const AnyExprRef&) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void valueAdded(UInt, const AnyExprRef&) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }

    void subsequenceChanged(UInt, UInt) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void positionsSwapped(UInt, UInt) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void memberHasBecomeDefined(UInt) { eventHandledAsDefinednessChange(); }
    void memberHasBecomeUndefined(UInt) { eventHandledAsDefinednessChange(); }
    void hasBecomeDefined() override { eventHandledAsDefinednessChange(); }
    void hasBecomeUndefined() override { eventHandledAsDefinednessChange(); }
};

template <typename Op>
struct ForwardingTrigger<SequenceTrigger, Op>
    : public ForwardingTriggerBase<SequenceTrigger, Op> {
    using ForwardingTriggerBase<SequenceTrigger, Op>::ForwardingTriggerBase;
    void valueRemoved(UInt index, const AnyExprRef& removedValue) {
        visitTriggers([&](auto& t) { t->valueRemoved(index, removedValue); },
                      this->op->triggers);
    }
    void valueAdded(UInt index, const AnyExprRef& expr) {
        visitTriggers([&](auto& t) { t->valueAdded(index, expr); },
                      this->op->triggers);
    }

    void subsequenceChanged(UInt startIndex, UInt endIndex) {
        visitTriggers(
            [&](auto& t) { t->subsequenceChanged(startIndex, endIndex); },
            this->op->triggers);
    }
    void memberHasBecomeUndefined(UInt index) {
        visitTriggers([&](auto& t) { t->memberHasBecomeUndefined(index); },
                      this->op->triggers);
    }
    void memberHasBecomeDefined(UInt index) {
        visitTriggers([&](auto& t) { t->memberHasBecomeDefined(index); },
                      this->op->triggers);
    }
    void positionsSwapped(UInt index1, UInt index2) {
        visitTriggers([&](auto& t) { t->positionsSwapped(index1, index2); },
                      this->op->triggers);
    }
};

#endif /* SRC_TRIGGERS_SEQUENCETRIGGER_H_ */
