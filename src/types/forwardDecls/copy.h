
#ifndef SRC_TYPES_FORWARDDECLS_COPY_H_
#define SRC_TYPES_FORWARDDECLS_COPY_H_
#include "types/forwardDecls/typesAndDomains.h"
#define makeDeepCopyDecl(name) \
    void deepCopy(const name##Value& src, name##Value& target);
buildForAllTypes(makeDeepCopyDecl, )
#undef makeDeepCopyDecl

    template <typename ValueType>
    ValRef<ValueType> deepCopy(const ValueType& src) {
    auto target = constructValueOfSameType(src);
    deepCopy(src, *target);
    return target;
}

inline Value deepCopyValue(const Value& val) {
    return mpark::visit(
        [](const auto& valImpl) { return Value(deepCopy(*valImpl)); }, val);
}

inline void deepCopyValue(const Value& u, Value& v) {
    if (u.index() == v.index()) {
        mpark::visit(
            [&](auto& uImpl) {
                auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                deepCopy(*uImpl, *vImpl);
            },
            u);
    } else {
        v = deepCopyValue(u);
    }
}
#endif /* SRC_TYPES_FORWARDDECLS_COPY_H_ */
