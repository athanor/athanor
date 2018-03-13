
#ifndef SRC_BASE_TRIGGERS_H_
#define SRC_BASE_TRIGGERS_H_
#include "base/exprRef.h"
template <typename T>
struct ExprRef;
struct TriggerBase {
    bool active;
};
struct DelayedTrigger : public virtual TriggerBase {
    virtual void trigger() = 0;
};
template <typename UnrollingView>
struct IterAssignedTrigger : public virtual TriggerBase {
    typedef UnrollingView ViewType;
    virtual void iterHasNewValue(const UnrollingView& oldValue,
                                 const ExprRef<UnrollingView>& newValue) = 0;
};

template <typename ValueType>
struct DefinedTrigger;

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

template <typename View, typename Operator>
View& getView(const Operator& op);
template <typename T>
class IterRef;
template <typename>
struct QuantifierView;

template <typename ExprType, typename Trigger>
inline void saveTriggerOverload(
    std::shared_ptr<QuantifierView<ExprType>>& quant,
    const std::shared_ptr<Trigger>& trigger) {
    quant->triggers.emplace_back(trigger);
}
template <typename Op, typename Trigger>
inline void saveTriggerOverload(Op& op,
                                const std::shared_ptr<Trigger>& trigger) {
    getView(op).triggers.emplace_back(trigger);

    mpark::visit(overloaded(
                     [&](IterRef<typename Trigger::ValueType>& ref) {
                         ref.getIterator().unrollTriggers.emplace_back(trigger);
                     },
                     [](auto&) {}),
                 op);
}

template <typename Op, typename Trigger>
void addTrigger(Op& op, const std::shared_ptr<Trigger>& trigger) {
    saveTriggerOverload(op, trigger);
    trigger->active = true;
}

template <typename Trigger>
void addDelayedTrigger(const std::shared_ptr<Trigger>& trigger) {
    delayedTriggerStack.emplace_back(trigger);
    delayedTriggerStack.back()->active = true;
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

#endif /* SRC_BASE_TRIGGERS_H_ */
