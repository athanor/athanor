
#ifndef SRC_BASE_TRIGGERS_H_
#define SRC_BASE_TRIGGERS_H_
#include <memory>
#include "base/exprRef.h"
#include "base/typeDecls.h"
#include "utils/ignoreUnused.h"
template <typename T>
struct ExprRef;
extern u_int64_t triggerEventCount;
struct TriggerBase {
    bool active = true;
    bool acceptDefinednessTriggers = true;
    virtual ~TriggerBase() {}

    virtual void valueChanged() = 0;
    virtual void reattachTrigger() = 0;
    virtual void hasBecomeUndefined() = 0;
    virtual void hasBecomeDefined() = 0;
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
                   std::vector<std::shared_ptr<Trigger>>& triggers,
                   bool isDefinednessTrigger = false) {
    size_t size = triggers.size();
    size_t triggerNullCount = 0;
    // triggers may be changed, insure that new triggers are ignored
    for (size_t i = 0; i < size && i < triggers.size(); i++) {
        auto& trigger = triggers[i];
        if (trigger && trigger->active) {
            if (isDefinednessTrigger && !trigger->acceptDefinednessTriggers) {
                continue;
            }
            ++triggerEventCount;
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

struct BoolTrigger : public virtual TriggerBase {
    inline void hasBecomeUndefined() final { shouldNotBeCalledPanic; }
    inline void hasBecomeDefined() { shouldNotBeCalledPanic; }
};

template <typename TriggerType, typename Child>
struct ChangeTriggerAdapterBaseHelper : public TriggerType {
    inline void hasBecomeDefined() final {
        static_cast<Child*>(this)->adapterHasBecomeDefined();
    }
    inline void hasBecomeUndefined() final {
        static_cast<Child*>(this)->adapterHasBecomeUndefined();
    }
};

template <typename Child>
struct ChangeTriggerAdapterBaseHelper<BoolTrigger, Child> : public BoolTrigger {
};

template <typename TriggerType, typename Child>
struct ChangeTriggerAdapterBase
    : public ChangeTriggerAdapterBaseHelper<TriggerType, Child> {
    inline void forwardValueChanged() {
        static_cast<Child*>(this)->adapterValueChanged();
    }
    inline void valueChanged() final { this->forwardValueChanged(); }
};

template <typename TriggerType, typename Child>
struct ChangeTriggerAdapter;
template <typename TriggerType>
const std::shared_ptr<TriggerBase> getTriggerBase(
    const std::shared_ptr<TriggerType>& trigger);
#endif /* SRC_BASE_TRIGGERS_H_ */
