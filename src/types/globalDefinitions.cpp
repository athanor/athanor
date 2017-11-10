
/*contains definitions for functions that use the same code for every type,
 * using macros to do the specialisation to avoid having to include this file,
 * which includes all the types */
#include "search/violationDescription.h"
#include "types/allTypes.h"
#define quote(x) #x

template <typename ValueType>
ValRef<ValueType> construct();

#define specialised(name)                                                      \
    std::vector<std::shared_ptr<name##Value>> memoryFor##name##s;              \
    template <>                                                                \
    ValRef<name##Value> construct<name##Value>() {                             \
        if (memoryFor##name##s.empty()) {                                      \
            return ValRef<name##Value>(std::make_shared<name##Value>());       \
        } else {                                                               \
            ValRef<name##Value> val(std::move(memoryFor##name##s.back()));     \
            memoryFor##name##s.pop_back();                                     \
            return val;                                                        \
        }                                                                      \
    }                                                                          \
    void saveMemory(std::shared_ptr<name##Value>& val) {                       \
        memoryFor##name##s.emplace_back(std::move(val));                       \
    }                                                                          \
                                                                               \
    void matchInnerType(const name##Domain& domain, name##Value& value);       \
    void matchInnerType(const name##Value& other, name##Value& value);         \
    ValRef<name##Value> constructValueFromDomain(const name##Domain& domain) { \
        auto val = construct<name##Value>();                                   \
        matchInnerType(domain, *val);                                          \
        return val;                                                            \
    }                                                                          \
    ValRef<name##Value> constructValueOfSameType(const name##Value& other) {   \
        auto val = construct<name##Value>();                                   \
        matchInnerType(other, *val);                                           \
        return val;                                                            \
    }                                                                          \
                                                                               \
    const std::string TypeAsString<name##Value>::value = quote(name##Value);   \
    const std::string TypeAsString<name##Domain>::value = quote(name##Domain); \
    void setId(name##Value& n, int64_t id) { n.id = id; }                      \
    int64_t getId(const name##Value& n) { return n.id; }                       \
    void updateViolationDescription(const name##Value& val,                    \
                                    u_int64_t parentViolation,                 \
                                    ViolationDescription& vioDesc) {           \
        vioDesc.addViolation(val.id, parentViolation);                         \
    }

buildForAllTypes(specialised, )
#undef specialised

    std::vector<std::shared_ptr<EndOfQueueTrigger>> emptyEndOfTriggerQueue;
