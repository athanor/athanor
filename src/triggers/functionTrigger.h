
#ifndef SRC_TRIGGERS_FUNCTIONTRIGGER_H_
#define SRC_TRIGGERS_FUNCTIONTRIGGER_H_
#include "base/base.h"
struct FunctionOuterTrigger : public virtual TriggerBase {
    // later will have more specific triggers, currently only those inherited
    // from TriggerBase
    virtual void memberHasBecomeDefined(UInt) = 0;
    virtual void memberHasBecomeUndefined(UInt) = 0;
};

struct FunctionMemberTrigger : public virtual TriggerBase {
    virtual void imageChanged(UInt index) = 0;

    virtual void imageChanged(const std::vector<UInt>& indices) = 0;
    virtual void imageSwap(UInt index1, UInt index2) = 0;
};

struct FunctionTrigger : public virtual FunctionOuterTrigger,
                         public virtual FunctionMemberTrigger {};

template <>
struct TriggerContainer<FunctionView>
    : public TriggerContainerBase<TriggerContainer<FunctionView>> {
    TriggerQueue<FunctionOuterTrigger> triggers;
    TriggerQueue<FunctionMemberTrigger> allMemberTriggers;
    std::vector<TriggerQueue<FunctionMemberTrigger>> singleMemberTriggers;

    void takeFrom(TriggerContainer<FunctionView>& other) {
        triggers.takeFrom(other.triggers);
        allMemberTriggers.takeFrom(other.allMemberTriggers);
        for (size_t i = 0; i < singleMemberTriggers.size(); i++) {
            if (i >= other.singleMemberTriggers.size()) {
                break;
            }
            singleMemberTriggers[i].takeFrom(other.singleMemberTriggers[i]);
        }
    }

    template <typename Func>
    void visitMemberTriggers(Func&& func, UInt index) {
        visitTriggers(func, allMemberTriggers);
        if (index < singleMemberTriggers.size()) {
            visitTriggers(func, singleMemberTriggers[index]);
        }
    }
    template <typename Func>
    void visitMemberTriggers(Func&& func, const std::vector<UInt>& indices) {
        visitTriggers(func, allMemberTriggers);
        for (auto& index : indices) {
            if (index < singleMemberTriggers.size()) {
                visitTriggers(func, singleMemberTriggers[index]);
            }
        }
    }

    void notifyMemberReplaced(UInt index, const AnyExprRef& oldMember) {
        visitMemberTriggers(
            [&](auto& t) { t->memberReplaced(index, oldMember); }, index);
    }

    inline void notifyImageChanged(UInt index) {
        visitMemberTriggers([&](auto& t) { t->imageChanged(index); }, index);
    }

    inline void notifyImagesChanged(const std::vector<UInt>& indices) {
        visitMemberTriggers([&](auto& t) { t->imageChanged(indices); },
                            indices);
    }

    inline void notifyImagesSwapped(UInt index1, UInt index2) {
        visitTriggers([&](auto& t) { t->imageSwap(index1, index2); },
                      allMemberTriggers);
        if (index1 < singleMemberTriggers.size()) {
            visitTriggers([&](auto& t) { t->imageSwap(index1, index2); },
                          singleMemberTriggers[index1]);
        }
        if (index2 < singleMemberTriggers.size()) {
            visitTriggers([&](auto& t) { t->imageSwap(index1, index2); },
                          singleMemberTriggers[index2]);
        }
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
struct ChangeTriggerAdapter<FunctionTrigger, Child>
    : public ChangeTriggerAdapterBase<FunctionTrigger, Child> {
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
    ChangeTriggerAdapter(const ExprRef<FunctionView>& op)
        : wasDefined(op->getViewIfDefined().hasValue()) {}
    void imageChanged(UInt) override {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }

    inline void memberReplaced(UInt, const AnyExprRef&) override {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }

    void imageChanged(const std::vector<UInt>&) override {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void imageSwap(UInt, UInt) override {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void hasBecomeDefined() override { eventHandledAsDefinednessChange(); }
    void hasBecomeUndefined() override { eventHandledAsDefinednessChange(); }

    void memberHasBecomeDefined(UInt) override {
        eventHandledAsDefinednessChange();
    }
    void memberHasBecomeUndefined(UInt) override {
        eventHandledAsDefinednessChange();
    }
};
template <typename Op, typename Child>
struct ForwardingTrigger<FunctionTrigger, Op, Child>
    : public ForwardingTriggerBase<FunctionTrigger, Op> {
    using ForwardingTriggerBase<FunctionTrigger, Op>::ForwardingTriggerBase;

    void reevaluateDefined() {
        auto view = static_cast<Child*>(this)
                        ->getTriggeringOperand()
                        ->getViewIfDefined();
        this->op->setAppearsDefined(view.hasValue());
    }

    void imageChanged(const std::vector<UInt>& indices) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyImagesChanged(indices);
        }
    }
    void imageChanged(UInt index) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyImageChanged(index);
        }
    }

    void memberReplaced(UInt index, const AnyExprRef& oldMember) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberReplaced(index, oldMember);
        }
    }

    void imageSwap(UInt index1, UInt index2) override {
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyImagesSwapped(index1, index2);
        }
    }
    void memberHasBecomeUndefined(UInt index) override {
        reevaluateDefined();
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberUndefined(index);
        }
    }
    void memberHasBecomeDefined(UInt index) override {
        reevaluateDefined();
        if (this->op->allowForwardingOfTrigger()) {
            this->op->notifyMemberDefined(index);
        }
    }
};

#endif /* SRC_TRIGGERS_FUNCTIONTRIGGER_H_ */
