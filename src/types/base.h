#ifndef SRC_TYPES_BASE_H_
#define SRC_TYPES_BASE_H_
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
#include "utils/cachedSharedPtr.h"
#include "utils/variantOperations.h"
#define buildForAllTypes(f, sep) f(Bool) sep f(Int) sep f(Set)

#define MACRO_COMMA ,

template <typename T>
using BaseType =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;
// forward declare structs
#define declDomainsAndValues(name) \
    struct name##Value;            \
    struct name##View;             \
    struct name##Domain;
buildForAllTypes(declDomainsAndValues, )
#undef declDomainsAndValues
    template <typename T>
    using ValRef = CachedSharedPtr<T>;

// short cut for building a variant of any other templated class, where the
// class is templated over a value (SetValue,IntValue, etc.)
#define variantValues(V) T<V##Value>
template <template <typename> class T>
using Variantised =
    mpark::variant<buildForAllTypes(variantValues, MACRO_COMMA)>;
#undef variantValues

// variant for values
using AnyValRef = Variantised<ValRef>;

// variant for domains
#define variantDomains(T) std::shared_ptr<T##Domain>
using Domain = mpark::variant<buildForAllTypes(variantDomains, MACRO_COMMA)>;
#undef variantDomains

template <typename T>
struct AssociatedDomain;
template <typename T>
struct AssociatedValueType;

template <typename T>
struct IsDomainPtrType : public std::false_type {};
template <typename T>
struct IsDomainType : public std::false_type {};
template <typename T>
struct IsValueType : public std::false_type {};

template <typename T>
struct TypeAsString;
#define makeAssociations(name)                                           \
    template <>                                                          \
    std::shared_ptr<name##Value> makeShared<name##Value>();              \
    void matchInnerType(const name##Domain& domain, name##Value& value); \
    void matchInnerType(const name##Value& other, name##Value& value);   \
    void reset(name##Value& value);                                      \
    template <>                                                          \
    struct AssociatedDomain<name##Value> {                               \
        typedef name##Domain type;                                       \
    };                                                                   \
                                                                         \
    template <>                                                          \
    struct AssociatedValueType<name##Domain> {                           \
        typedef name##Value type;                                        \
    };                                                                   \
    template <>                                                          \
    struct TypeAsString<name##Value> {                                   \
        static const std::string value;                                  \
    };                                                                   \
    template <>                                                          \
    struct TypeAsString<name##Domain> {                                  \
        static const std::string value;                                  \
    };                                                                   \
                                                                         \
    template <>                                                          \
    struct IsDomainPtrType<std::shared_ptr<name##Domain>>                \
        : public std::true_type {};                                      \
                                                                         \
    template <>                                                          \
    struct IsDomainType<name##Domain> : public std::true_type {};        \
    template <>                                                          \
    struct IsValueType<name##Value> : public std::true_type {};

buildForAllTypes(makeAssociations, )
#undef makeAssociations

    struct ValBase {
    int64_t id = -1;
};
#define valBaseAccessors(name)           \
    int64_t getId(const name##Value& n); \
    void setId(name##Value& n, int64_t id);
buildForAllTypes(valBaseAccessors, )
#undef valBaseAccessors

    template <typename DomainType>
    ValRef<typename AssociatedValueType<
        DomainType>::type> constructValueFromDomain(const DomainType& domain) {
    auto val = make<typename AssociatedValueType<DomainType>::type>();
    matchInnerType(domain, *val);
    return val;
}
template <typename ValueType>
ValRef<ValueType> constructValueOfSameType(const ValueType& other) {
    auto val = make<ValueType>();
    matchInnerType(other, *val);
    return val;
}

struct TriggerBase {
    bool active;
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
void addTrigger(std::vector<std::shared_ptr<Trigger>>& triggerVec,
                const std::shared_ptr<Trigger>& trigger) {
    triggerVec.emplace_back(trigger);
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
#endif
