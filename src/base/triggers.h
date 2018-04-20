

#ifndef SRC_BASE_TRIGGERS_H_
#define SRC_BASE_TRIGGERS_H_
#include <memory>
#include "base/exprRef.h"
#include "base/typeDecls.h"
#include "utils/ignoreUnused.h"

template <typename T>
struct ExprRef;

struct TriggerBase {
    bool active = true;
    virtual void possibleValueChange() = 0;
    virtual void valueChanged() = 0;
    virtual void reattachTrigger() = 0;
};

struct DelayedTrigger : public virtual TriggerBase {
    virtual void trigger() = 0;
};

extern std::vector<std::shared_ptr<DelayedTrigger>> delayedTriggerStack;
extern bool currentlyProcessingDelayedTriggerStack;

template <typename Trigger>
void cleanNullTriggers(std::vector<std::shared_ptr<Trigger>>& triggers) {
    for (size_t i = 0; i < triggers.size(); ++i) {
        if (triggers[i]) {
            continue;
        }
        while (!triggers.empty() && !triggers.back()) {
            triggers.pop_back();
        }
        if (i >= triggers.size()) {
            break;
        } else {
            triggers[i] = std::move(triggers.back());
            triggers.pop_back();
        }
    }
}

template <typename Visitor, typename Trigger>
void visitTriggers(Visitor&& func,
                   std::vector<std::shared_ptr<Trigger>>& triggers) {
    size_t triggerNullCount = 0;
    for (auto& trigger : triggers) {
        if (trigger && trigger->active) {
            func(trigger);
        } else {
            ++triggerNullCount;
            if (trigger && !trigger->active) {
                trigger = nullptr;
            }
        }
    }
    if (((double)triggerNullCount) / triggers.size() > 0.2) {
        cleanNullTriggers(triggers);
    }
    if (!currentlyProcessingDelayedTriggerStack) {
        currentlyProcessingDelayedTriggerStack = true;
        while (!delayedTriggerStack.empty()) {
            auto trigger = std::move(delayedTriggerStack.back());
            delayedTriggerStack.pop_back();
            if (trigger && trigger->active) {
                trigger->trigger();
            }
        }
        currentlyProcessingDelayedTriggerStack = false;
    }
}

template <typename Trigger>
void addDelayedTrigger(const std::shared_ptr<Trigger>& trigger) {
    delayedTriggerStack.emplace_back(trigger);
}

template <typename Trigger>
void deleteTrigger(const std::shared_ptr<Trigger>& trigger) {
    trigger->active = false;
}

template <typename Op, typename Trigger>
void setTriggerParentImpl(Op* op, std::shared_ptr<Trigger>& trigger) {
    if (trigger) {
        trigger->op = op;
    }
}
template <typename Op, typename Trigger>
void setTriggerParentImpl(Op* op,
                          std::vector<std::shared_ptr<Trigger>>& triggers) {
    for (auto& trigger : triggers) {
        trigger->op = op;
    }
}

template <typename Op, typename... Triggers>
void setTriggerParent(Op* op, Triggers&... triggers) {
    int unpack[] = {0, (setTriggerParentImpl(op, triggers), 0)...};
    static_cast<void>(unpack);
}

template <typename Child>
struct ChangeTriggerAdapterBase {
    inline void forwardPossibleValueChange() {
        static_cast<Child*>(this)->adapterPossibleValueChange();
    }

    inline void forwardValueChanged() {
        static_cast<Child*>(this)->adapterValueChanged();
    }
};

template <typename TriggerType, typename Child>
struct ChangeTriggerAdapter;
#endif /* SRC_BASE_TRIGGERS_H_ */
