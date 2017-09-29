#include "types/allTypes.h"
#include "types/forwardDecls/changeValue.h"
#define specialisedConstructors(name)                       \
    template <>                                             \
    std::shared_ptr<name##Value> construct<name##Value>() { \
        return std::make_shared<name##Value>();             \
    }

buildForAllTypes(specialisedConstructors, )
#undef specialisedConstructors
