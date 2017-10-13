
#ifndef SRC_TYPES_FORWARDDECLS_COPY_H_
#define SRC_TYPES_FORWARDDECLS_COPY_H_
#include "types/forwardDecls/typesAndDomains.h"
#define makeDeepCopyDecl(name) \
    void deepCopy(const name##Value& src, name##Value& target);
buildForAllTypes(makeDeepCopyDecl, )
#undef makeDeepCopyDecl

    template <typename ValueType>
    ValRef<ValueType> deepCopy(const ValueType& src) {
    auto target = construct<ValueType>();
    matchInnerType(src, *target);
    deepCopy(src, *target);
    return target;
}

inline Value deepCopyValue(const Value& val) {
    return mpark::visit([] (const auto& valImpl) { return Value(deepCopy(*valImpl)); },
                        val);
}

#endif /* SRC_TYPES_FORWARDDECLS_COPY_H_ */
