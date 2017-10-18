
#ifndef SRC_TYPES_FORWARDDECLS_COMPARE_H_
#define SRC_TYPES_FORWARDDECLS_COMPARE_H_
#include "types/forwardDecls/typesAndDomains.h"

#include <iostream>

#define CompareDecl(name)                                          \
    bool smallerValue(const name##Value& u, const name##Value& v); \
    bool largerValue(const name##Value& u, const name##Value& v);  \
    bool equalValue(const name##Value& u, const name##Value& v);   \
    void normalise(name##Value& v);
buildForAllTypes(CompareDecl, )
#undef makePrettyPrintDecl

    inline bool smallerValue(const Value& u, const Value& v) {
    return u.index() < v.index() ||
           (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return smallerValue(*uImpl, *vImpl);
                },
                u));
}

inline void normalise(Value& v) {
    mpark::visit([](auto& vImpl) { normalise(*vImpl); }, v);
}
inline bool largerValue(const Value& u, const Value& v) {
    return u.index() > v.index() ||
           (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return largerValue(*uImpl, *vImpl);
                },
                u));
}

inline bool equalValue(const Value& u, const Value& v) {
    return (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return equalValue(*uImpl, *vImpl);
                },
                u));
}

#endif
