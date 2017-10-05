
#ifndef SRC_TYPES_FORWARDDECLS_CONSTRUCTVALUE_H_
#define SRC_TYPES_FORWARDDECLS_CONSTRUCTVALUE_H_
#include "types/forwardDecls/typesAndDomains.h"

template <typename ValueType>
std::shared_ptr<ValueType> construct();

#define changeValueFunctions(name)                         \
    template <>                                            \
    std::shared_ptr<name##Value> construct<name##Value>(); \
    void matchInnerType(const name##Value&, name##Value&); \
    \
    void                                                   \
    matchInnerType(const name##Domain&, name##Value&);

buildForAllTypes(changeValueFunctions, )
#undef changeValueFunctions

#endif /* SRC_TYPES_FORWARDDECLS_CONSTRUCTVALUE_H_ */
