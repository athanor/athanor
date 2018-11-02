
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

template <>
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
    inline void notifyMemberAdded(size_t index, const AnyExprRef& newMember) {
        visitTriggers([&](auto& t) { t->valueAdded(index, newMember); },
                      triggers);
    }

    inline void notifyMemberRemoved(UInt index,
                                    const AnyExprRef& removedMember) {
        visitTriggers([&](auto& t) { t->valueRemoved(index, removedMember); },
                      triggers);
    }

    inline void notifyPositionsSwapped(UInt index1, UInt index2) {
        visitTriggers([&](auto& t) { t->positionsSwapped(index1, index2); },
                      allMemberTriggers);
        if (index1 < singleMemberTriggers.size()) {
            visitTriggers([&](auto& t) { t->positionsSwapped(index1, index2); },
                          singleMemberTriggers[index1]);
        }
        if (index2 < singleMemberTriggers.size()) {
            visitTriggers([&](auto& t) { t->positionsSwapped(index1, index2); },
                          singleMemberTriggers[index2]);
        }
    }

    inline void notifySubsequenceChanged(UInt startIndex, UInt endIndex) {
        visitAllMemberTriggersInRange(
            [&](auto& t) { t->subsequenceChanged(startIndex, endIndex); },
            startIndex, endIndex);
    }

    inline void notifyMemberDefined(UInt index) {
        visitTriggers([&](auto& t) { t->memberHasBecomeDefined(index); },
                      triggers);
    }

    inline void notifyMemberUndefined(UInt index) {
        visitTriggers([&](auto& t) { t->memberHasBecomeUndefined(index); },
                      triggers);
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
size_t numberElements(SequenceView& view);
template <typename Op, typename Child>
struct ForwardingTrigger<SequenceTrigger, Op, Child>
    : public ForwardingTriggerBase<SequenceTrigger, Op> {
    using ForwardingTriggerBase<SequenceTrigger, Op>::ForwardingTriggerBase;

   public:
    void reevaluateDefined() {
        auto view = static_cast<Child*>(this)
                        ->getTriggeringOperand()
                        ->getViewIfDefined();
        this->op->setAppearsDefined(view.hasValue());
    }

   public:
    void valueRemoved(UInt index, const AnyExprRef& removedValue) {
        auto& view =
            static_cast<Child*>(this)->getTriggeringOperand()->view().get();
        if (numberElements(view) < this->op->singleMemberTriggers.size()) {
            this->op->singleMemberTriggers.resize(numberElements(view));
        }
        reevaluateDefined();
        this->op->notifyMemberRemoved(index, removedValue);
    }

    void valueAdded(UInt index, const AnyExprRef& expr) {
        if (!appearsDefined(expr)) {
            this->op->setAppearsDefined(false);
        }
        this->op->notifyMemberAdded(index, expr);
    }

    void subsequenceChanged(UInt startIndex, UInt endIndex) {
        this->op->notifySubsequenceChanged(startIndex, endIndex);
    }

    void memberHasBecomeUndefined(UInt index) {
        reevaluateDefined();
        this->op->notifyMemberUndefined(index);
    }
    void memberHasBecomeDefined(UInt index) {
        reevaluateDefined();
        this->op->notifyMemberDefined(index);
    }
    void positionsSwapped(UInt index1, UInt index2) {
        this->op->notifyPositionsSwapped(index1, index2);
    }
};

#endif /* SRC_TRIGGERS_SEQUENCETRIGGER_H_ */
