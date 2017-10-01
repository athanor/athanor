
#ifndef SRC_TYPES_FORWARDDECLS_CHANGEVALUE_H_
#define SRC_TYPES_FORWARDDECLS_CHANGEVALUE_H_
#include "types/forwardDecls/typesAndDomains.h"
#include "types/parentCheck.h"

template <typename ValueType>
std::shared_ptr<ValueType> construct();

#define changeValueFunctions(name)                                         \
    template <>                                                            \
    std::shared_ptr<name##Value> construct<name##Value>();                 \
    void assignInitialValueInDomain(const name##Domain& domain,            \
                                    name##Value& value);                   \
    std::shared_ptr<name##Value> makeInitialValueInDomain(                 \
        const name##Domain& domain);                                       \
    bool moveToNextValueInDomain(name##Value&, const name##Domain& domain, \
                                 const ParentCheck& parentCheck);
buildForAllTypes(changeValueFunctions, )
#undef changeValueFunctions

#endif /* SRC_TYPES_FORWARDDECLS_CHANGEVALUE_H_ */
