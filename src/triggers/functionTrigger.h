
#ifndef SRC_TRIGGERS_FUNCTIONTRIGGER_H_
#define SRC_TRIGGERS_FUNCTIONTRIGGER_H_
#include "base/base.h"
struct FunctionOuterTrigger : public virtual TriggerBase {
    // later will have more specific triggers, currently only those inherited
    // from TriggerBase
};

struct FunctionMemberTrigger : public virtual TriggerBase {
    virtual void imageChanged(UInt index) = 0;

    virtual void imageChanged(const std::vector<UInt>& indices) = 0;
    virtual void imageSwap(UInt index1, UInt index2) = 0;
};

struct FunctionTrigger : public virtual FunctionOuterTrigger,
                         public virtual FunctionMemberTrigger {};

template <>
struct TriggerContainer<FunctionView> {
    std::vector<std::shared_ptr<FunctionOuterTrigger>> triggers;
    std::vector<std::shared_ptr<FunctionMemberTrigger>> allMemberTriggers;
    std::vector<std::vector<std::shared_ptr<FunctionMemberTrigger>>>
        singleMemberTriggers;
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
    void imageChanged(UInt) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void imageChanged(const std::vector<UInt>&) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void imageSwap(UInt, UInt) {
        if (!eventHandledAsDefinednessChange()) {
            this->forwardValueChanged();
        }
    }
    void hasBecomeDefined() { eventHandledAsDefinednessChange(); }
    void hasBecomeUndefined() { eventHandledAsDefinednessChange(); }
};
template <typename Op, typename Child>
struct ForwardingTrigger<FunctionTrigger, Op, Child>
    : public ForwardingTriggerBase<FunctionTrigger, Op> {
    using ForwardingTriggerBase<FunctionTrigger, Op>::ForwardingTriggerBase;
    void imageChanged(const std::vector<UInt>& indices) {
        this->op->notifyImagesChanged(indices);
    }
    void imageChanged(UInt index) { this->op->notifyImageChanged(index); }

    void imageSwap(UInt index1, UInt index2) {
        this->op->notifyImagesSwapped(index1, index2);
    }
};

#endif /* SRC_TRIGGERS_FUNCTIONTRIGGER_H_ */
