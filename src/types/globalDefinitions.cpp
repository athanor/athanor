
/*contains definitions for functions that use the same code for every type,
 * using macros to do the specialisation to avoid having to include this file,
 * which includes all the types */
#include "search/violationDescription.h"
#include "types/allTypes.h"
#define quote(x) #x

#define specialised(name)                                                      \
    template <>                                                                \
    std::shared_ptr<name##Value> makeShared<name##Value>() {                   \
        return std::make_shared<name##Value>();                                \
    }                                                                          \
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

    std::vector<std::shared_ptr<DelayedTrigger>> emptyEndOfTriggerQueue;
