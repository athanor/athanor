
#ifndef SRC_BASE_TRIGGERS_H_
#define SRC_BASE_TRIGGERS_H_
#include <memory>
#include "base/exprRef.h"
#include "base/typeDecls.h"
#include "utils/flagSet.h"
#include "utils/ignoreUnused.h"
template <typename T>
struct ExprRef;
extern u_int64_t triggerEventCount;
struct TriggerBase {
    struct TRIGGER_ACTIVE;
    struct ALLOW_DEFINEDNESS_TRIGGERS;
    struct ALLOW_ONLY_DEFINEDNESS_TRIGGERS;
    FlagSet<TRIGGER_ACTIVE, ALLOW_DEFINEDNESS_TRIGGERS,
            ALLOW_ONLY_DEFINEDNESS_TRIGGERS>
        flags = {{true, true, false}};

    virtual ~TriggerBase() {}

    virtual void valueChanged() = 0;
    virtual void reattachTrigger() = 0;
    virtual void hasBecomeUndefined() = 0;
    virtual void hasBecomeDefined() = 0;
};

struct DelayedTrigger : public virtual TriggerBase {
    virtual void trigger() = 0;
};

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

struct DelayedTriggerStack;
extern DelayedTriggerStack* currentDelayedTriggerStack;

struct DelayedTriggerStack {
    std::vector<std::shared_ptr<DelayedTrigger>> stack;
    DelayedTriggerStack* oldStack;
    DelayedTriggerStack() : oldStack(currentDelayedTriggerStack) {
        currentDelayedTriggerStack = this;
    }
    ~DelayedTriggerStack() {
        debug_code(assert(currentDelayedTriggerStack == this));
        currentDelayedTriggerStack = oldStack;
    }
};

template <typename Visitor, typename Trigger>
void visitTriggers(Visitor&& func,
                   std::vector<std::shared_ptr<Trigger>>& triggers,
                   bool isDefinednessTrigger = false) {
    using ALLOW_DEFINEDNESS_TRIGGERS = TriggerBase::ALLOW_DEFINEDNESS_TRIGGERS;
    using ALLOW_ONLY_DEFINEDNESS_TRIGGERS =
        TriggerBase::ALLOW_ONLY_DEFINEDNESS_TRIGGERS;
    using TRIGGER_ACTIVE = TriggerBase::TRIGGER_ACTIVE;
    DelayedTriggerStack delayedTriggers;

    size_t size = triggers.size();
    size_t triggerNullCount = 0;
    // triggers may be changed, insure that new triggers are ignored
    for (size_t i = 0; i < size && i < triggers.size(); i++) {
        auto& trigger = triggers[i];
        if (trigger && trigger->flags.template get<TRIGGER_ACTIVE>()) {
            if ((isDefinednessTrigger &&
                 !trigger->flags.template get<ALLOW_DEFINEDNESS_TRIGGERS>()) ||
                (!isDefinednessTrigger &&
                 trigger->flags
                     .template get<ALLOW_ONLY_DEFINEDNESS_TRIGGERS>())) {
                continue;
            }
            ++triggerEventCount;
            func(trigger);
        } else {
            ++triggerNullCount;
            trigger = nullptr;
        }
    }
    if (((double)triggerNullCount) / triggers.size() > 0.2) {
        cleanNullTriggers(triggers);
    }

    while (!delayedTriggers.stack.empty()) {
        auto trigger = std::move(delayedTriggers.stack.back());
        delayedTriggers.stack.pop_back();
        if (trigger && trigger->flags.template get<TRIGGER_ACTIVE>()) {
            trigger->trigger();
        }
    }
}

template <typename Trigger>
void addDelayedTrigger(const std::shared_ptr<Trigger>& trigger) {
    debug_code(assert(currentDelayedTriggerStack != NULL));
    currentDelayedTriggerStack->stack.emplace_back(trigger);
}

template <typename Trigger>
void deleteTrigger(const std::shared_ptr<Trigger>& trigger) {
    trigger->flags.template get<TriggerBase::TRIGGER_ACTIVE>() = false;
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
    inline void forwardHasBecomeDefined() {
        static_cast<Child*>(this)->adapterHasBecomeDefined();
    }
    inline void hasBecomeDefined() { forwardHasBecomeDefined(); }
    inline void forwardHasBecomeUndefined() {
        static_cast<Child*>(this)->adapterHasBecomeUndefined();
    }
    inline void hasBecomeUndefined() { forwardHasBecomeUndefined(); }
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
