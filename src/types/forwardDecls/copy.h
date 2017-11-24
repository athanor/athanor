
#ifndef SRC_TYPES_FORWARDDECLS_COPY_H_
#define SRC_TYPES_FORWARDDECLS_COPY_H_
#include "types/base.h"
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

inline AnyValRef deepCopyValue(const AnyValRef& val) {
    return mpark::visit(
        [](const auto& valImpl) { return AnyValRef(deepCopy(*valImpl)); }, val);
}

inline void deepCopyValue(const AnyValRef& u, AnyValRef& v) {
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
