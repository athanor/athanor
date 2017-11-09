#ifndef SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_
#define SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_
#include <utils/stackFunction.h>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
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

    // ref type used for values, allows memory to be reused
    template <typename T>
    class ValRef;

// method declarations used to construct ValRefs as well as methods to reset
// their state when saving their memory
// These methods don't have to be individually implemented in each type, they
// are implemented already in globalDefinitions.h.

#define constructFunctions(name)                                       \
    ValRef<name##Value> constructValueFromDomain(const name##Domain&); \
    ValRef<name##Value> constructValueOfSameType(const name##Value&);  \
    void reset(name##Value& value);                                    \
    void saveMemory(std::shared_ptr<name##Value>& ref);

buildForAllTypes(constructFunctions, )
#undef constructFunctions

    template <typename T>
    class ValRef {
   public:
    typedef T element_type;

   private:
    std::shared_ptr<T> ref;

   public:
    ValRef(std::shared_ptr<T> ref) : ref(std::move(ref)) {}
    ~ValRef() {
        if (!ref) {
            return;
        }
        if (ref.use_count() == 1) {
            reset(*ref);
            saveMemory(ref);
        }
    }
    inline decltype(auto) operator*() { return ref.operator*(); }
    inline decltype(auto) operator-> () { return ref.operator->(); }
    inline decltype(auto) operator*() const { return ref.operator*(); }
    inline decltype(auto) operator-> () const { return ref.operator->(); }
};

// short cut for building a variant of any other templated class, where the
// class is templated over a value (SetValue,IntValue, etc.)
#define variantValues(V) T<V##Value>
template <template <typename> class T>
using Variantised =
    mpark::variant<buildForAllTypes(variantValues, MACRO_COMMA)>;
#undef variantValues

// variant for values
using Value = Variantised<ValRef>;

// variant for domains
#define variantDomains(T) std::shared_ptr<T##Domain>
using Domain = mpark::variant<buildForAllTypes(variantDomains, MACRO_COMMA)>;
#undef variantDomains

struct ValBase {
    int64_t id = -1;
};
#define valBaseAccessors(name)           \
    int64_t getId(const name##Value& n); \
    void setId(name##Value& n, int64_t id);
buildForAllTypes(valBaseAccessors, )
#undef valBaseAccessors

    struct TriggerBase {
    size_t indexInVector;
};
struct EndOfQueueTrigger {
    virtual void reachedEndOfTriggerQueue() = 0;
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
            triggers[i]->indexInVector = i;
            triggers.pop_back();
        }
    }
}

template <typename Trigger, typename Visitor>
void visitTriggers(Visitor&& func,
                   std::vector<std::shared_ptr<Trigger>>& triggers) {
    size_t triggerNullCount = 0;
    for (auto& trigger : triggers) {
        if (trigger) {
            func(trigger);
        } else {
            ++triggerNullCount;
        }
    }
    if (((double)triggerNullCount) / triggers.size() > 0.2) {
        cleanNullTriggers(triggers);
    }
}
template <typename Trigger>
void addTrigger(std::vector<std::shared_ptr<Trigger>>& triggerVec,
                const std::shared_ptr<Trigger>& trigger) {
    triggerVec.emplace_back(trigger);
    static_cast<TriggerBase&>(*trigger).indexInVector = triggerVec.size() - 1;
}

template <typename Trigger>
void deleteTrigger(std::vector<std::shared_ptr<Trigger>>& triggerVec,
                   const std::shared_ptr<Trigger>& trigger) {
    size_t indexToDelete = trigger->indexInVector;
    triggerVec[indexToDelete] = nullptr;
}

#endif /* SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_ */
