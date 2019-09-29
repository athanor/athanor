
#ifndef SRC_BASE_TRIGGERS_H_
#define SRC_BASE_TRIGGERS_H_
#include <memory>
#include "base/exprRef.h"
#include "base/typeDecls.h"
#include "utils/flagSet.h"
#include "utils/ignoreUnused.h"
template <typename T>
struct ExprRef;
extern UInt64 triggerEventCount;
struct TriggerBase {
   private:
    bool _active = true;

   public:
    bool& active() { return _active; }
    virtual ~TriggerBase() {}

    virtual void valueChanged() = 0;
    virtual void reattachTrigger() = 0;
    virtual void hasBecomeUndefined() = 0;
    virtual void hasBecomeDefined() = 0;
    virtual void memberReplaced(UInt index, const AnyExprRef& oldMember) = 0;
};

static const size_t MIN_CLEAN_SIZE = 16;

template <typename T>
class TriggerQueue;

template <typename T>
class TriggerQueue {
    bool currentlyProcessing = false;
    size_t lastCleanSize = 0;
    std::deque<std::shared_ptr<T>> triggers;

   public:
    struct QueueAccess {
        friend class TriggerQueue<T>;

       private:
        TriggerQueue<T>& queue;
        bool firstAccess;

       public:
        std::deque<std::shared_ptr<T>>& triggers;

       private:
        QueueAccess(TriggerQueue<T>& queue)
            : queue(queue), triggers(queue.triggers) {
            firstAccess = !queue.currentlyProcessing;
            queue.currentlyProcessing = true;
        }

       public:
        ~QueueAccess() {
            if (firstAccess) {
                queue.currentlyProcessing = false;
            }
        }
    };

   public:
    inline QueueAccess access() { return QueueAccess(*this); }

    void takeFrom(TriggerQueue<T>& other) {
        triggers.insert(triggers.end(), other.triggers.begin(),
                        other.triggers.end());
        other.triggers.clear();
    }
    template <typename Trigger>
    void add(Trigger&& trigger) {
        if (!currentlyProcessing &&
            triggers.size() > lastCleanSize * 2 + MIN_CLEAN_SIZE) {
            cleanNullTriggers(true);
        }
        triggers.emplace_back(std::forward<Trigger>(trigger));
    }

    void cleanNullTriggers(bool includeInactive = false) {
        for (size_t i = 0; i < triggers.size(); ++i) {
            if (triggers[i] && (!includeInactive || triggers[i]->active())) {
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
        lastCleanSize = triggers.size();
    }
};

template <typename TrigCont, typename Trigger>
auto addAsMemberTrigger(TrigCont& container,
                        const std::shared_ptr<Trigger>& trigger,
                        Int memberIndex)
    -> decltype(container.allMemberTriggers, void()) {
    if (memberIndex == -1) {
        container.allMemberTriggers.add(trigger);
    } else {
        if ((Int)container.singleMemberTriggers.size() <= memberIndex) {
            container.singleMemberTriggers.resize(memberIndex + 1);
        }
        container.singleMemberTriggers[memberIndex].add(trigger);
    }
}

template <typename... Args>
auto addAsMemberTrigger(Args&&...) {}

template <typename TrigCont, typename Trigger>
void handleTriggerAdd(const std::shared_ptr<Trigger>& trigger,
                      bool includeMembers, Int memberIndex,
                      TrigCont& container) {
    container.triggers.add(trigger);
    if (includeMembers) {
        addAsMemberTrigger(container, trigger, memberIndex);
    }
}

class TriggerDepthTracker {
    static int globalDepth;

   public:
    TriggerDepthTracker() { ++globalDepth; }
    ~TriggerDepthTracker() { --globalDepth; }
    inline int depth() { return globalDepth; }
    inline bool atBottom() { return depth() == 0; }
};

void handleDefinedVarTriggers();
template <typename Visitor, typename Trigger>
void visitTriggers(Visitor&& func, TriggerQueue<Trigger>& queue) {
    TriggerDepthTracker triggerDepth;
    auto access = queue.access();

    size_t size = access.triggers.size();
    size_t triggerNullCount = 0;
    // triggers may be changed, insure that new triggers are ignored
    for (size_t i = 0; i < size && i < access.triggers.size(); i++) {
        auto& trigger = access.triggers[i];
        if (trigger && trigger->active()) {
            ++triggerEventCount;
            func(trigger);
        } else {
            ++triggerNullCount;
            trigger = nullptr;
        }
    }
    if (access.triggers.size() > MIN_CLEAN_SIZE &&
        ((double)triggerNullCount) / access.triggers.size() > 0.5) {
        queue.cleanNullTriggers();
    }
    if (triggerDepth.atBottom()) {
        // outer most visit triggers call
        handleDefinedVarTriggers();
    }
}

template <typename Trigger>
void deleteTrigger(const std::shared_ptr<Trigger>& trigger) {
    if (trigger) {
        trigger->active() = false;
    }
}

struct BoolTrigger : public virtual TriggerBase {
    void hasBecomeUndefined() override { shouldNotBeCalledPanic; }
    void hasBecomeDefined() override { shouldNotBeCalledPanic; }
    inline void memberReplaced(UInt, const AnyExprRef&) override {
        shouldNotBeCalledPanic;
    }
};

template <typename TriggerType, typename Child>
struct ChangeTriggerAdapterBaseHelper : public TriggerType {
    void forwardHasBecomeDefined() {
        static_cast<Child*>(this)->adapterHasBecomeDefined();
    }
    void hasBecomeDefined() { forwardHasBecomeDefined(); }
    void forwardHasBecomeUndefined() {
        static_cast<Child*>(this)->adapterHasBecomeUndefined();
    }
    void hasBecomeUndefined() { forwardHasBecomeUndefined(); }
};

template <typename Child>
struct ChangeTriggerAdapterBaseHelper<BoolTrigger, Child> : public BoolTrigger {
};

template <typename TriggerType, typename Child>
struct ChangeTriggerAdapterBase
    : public ChangeTriggerAdapterBaseHelper<TriggerType, Child> {
    void forwardValueChanged() {
        static_cast<Child*>(this)->adapterValueChanged();
    }
    void valueChanged() { this->forwardValueChanged(); }
};

template <typename TriggerType, typename Child>
struct ChangeTriggerAdapter;

template <typename TriggerType>
const std::shared_ptr<TriggerBase> getTriggerBase(
    const std::shared_ptr<TriggerType>& trigger);

template <typename TriggerType, typename Op>
struct ForwardingTriggerBase : public virtual TriggerType {
    Op* op;
    ForwardingTriggerBase(Op* op) : op(op) {}

    void valueChanged() override {
        if (op->allowForwardingOfTrigger()) {
            visitTriggers([&](auto& t) { t->valueChanged(); }, op->triggers);
        }
    }
    void hasBecomeDefined() override {
        op->setAppearsDefined(true);
        if (op->allowForwardingOfTrigger()) {
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                          op->triggers);
        }
    }
    void hasBecomeUndefined() override {
        op->setAppearsDefined(false);
        if (op->allowForwardingOfTrigger()) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        }
    }
};
template <typename Trigger, typename Op, typename Child>
struct ForwardingTrigger;

template <typename View>
struct TriggerContainer;

template <typename Derived>
struct TriggerContainerBase {
    void notifyEntireValueChanged() {
        visitTriggers([&](auto& t) { t->valueChanged(); },
                      static_cast<Derived&>(*this).triggers);
    }
    void notifyValueDefined() {
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                      static_cast<Derived&>(*this).triggers);
    }
    void notifyValueUndefined() {
        visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                      static_cast<Derived&>(*this).triggers);
    }
    void notifyReattachTrigger() {
        visitTriggers([&](auto& t) { t->reattachTrigger(); },
                      static_cast<Derived&>(*this).triggers);
    }
};

#endif /* SRC_BASE_TRIGGERS_H_ */
