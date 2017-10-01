
#ifndef SRC_TYPES_FORWARDDECLS_PRINT_H_
#define SRC_TYPES_FORWARDDECLS_PRINT_H_
#include "types/forwardDecls/typesAndDomains.h"

#include <iostream>

#define makePrettyPrintDecl(name)                                    \
    std::ostream& prettyPrint(std::ostream& os, const name##Value&); \
    std::ostream& prettyPrint(std::ostream& os, const name##Domain&);
buildForAllTypes(makePrettyPrintDecl, )
#undef makePrettyPrintDecl

    inline std::ostream& prettyPrint(std::ostream& os, const Domain& d) {
    mpark::visit([&os](auto& dImpl) { prettyPrint(os, *dImpl); }, d);
    return os;
}
inline std::ostream& prettyPrint(std::ostream& os, const Value& v) {
    mpark::visit([&os](auto& vImpl) { prettyPrint(os, *vImpl); }, v);
    return os;
}

template <typename Value,
          typename std::enable_if<
              std::is_same<typename AssociatedDomain<Value>::type,
                           typename AssociatedDomain<Value>::type>::value,
              int>::type = 0>
std::ostream& operator<<(std::ostream& os, const std::shared_ptr<Value>& v) {
    return prettyPrint(os, v);
}
#endif
