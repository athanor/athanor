
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

template <typename Value>
std::ostream& operator<<(std::ostream& os, const ValRef<Value>& v) {
    return prettyPrint(os, v);
}

inline std::ostream& operator<<(std::ostream& os, const Value& v) {
    return prettyPrint(os, v);
}
#endif
