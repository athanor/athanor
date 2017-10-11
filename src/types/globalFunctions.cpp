
/*contains definitions for functions that use the same code for every type,
 * using macros to do the specialisation to avoid having to include this file,
 * which includes all the types */
#include "types/allTypes.h"
#define quote(x) #x

#define specialised(name)                                                    \
    std::vector<std::shared_ptr<name##Value>> memoryFor##name##s;            \
    template <>                                                              \
    ValRef<name##Value> construct<name##Value>() {                           \
        if (memoryFor##name##s.empty()) {                                    \
            return ValRef<name##Value>(std::make_shared<name##Value>());     \
        } else {                                                             \
            ValRef<name##Value> val(std::move(memoryFor##name##s.back()));   \
            memoryFor##name##s.pop_back();                                   \
            return val;                                                      \
        }                                                                    \
    }                                                                        \
    void saveMemory(std::shared_ptr<name##Value>& val) {                     \
        memoryFor##name##s.emplace_back(std::move(val));                     \
    }                                                                        \
    const std::string TypeAsString<name##Value>::value = quote(name##Value); \
    const std::string TypeAsString<name##Domain>::value = quote(name##Domain);

buildForAllTypes(specialised, )
#undef specialised
