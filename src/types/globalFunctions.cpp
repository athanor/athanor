
/*contains definitions for functions that use the same code for every type,
 * using macros to do the specialisation to avoid having to include this file,
 * which includes all the types */
#include "types/allTypes.h"
#include "types/forwardDecls/changeValue.h"
#define quote(x) #x

#define specialised(name)                                                    \
    template <>                                                              \
    std::shared_ptr<name##Value> construct<name##Value>() {                  \
        return std::make_shared<name##Value>();                              \
    }                                                                        \
    const std::string TypeAsString<name##Value>::value = quote(name##Value); \
    const std::string TypeAsString<name##Domain>::value = quote(name##Domain);

buildForAllTypes(specialised, )
#undef specialised
