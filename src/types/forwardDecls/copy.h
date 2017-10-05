
#ifndef SRC_TYPES_FORWARDDECLS_COPY_H_
#define SRC_TYPES_FORWARDDECLS_COPY_H_
#include "types/forwardDecls/constructValue.h"
#include "types/forwardDecls/typesAndDomains.h"
#define makeDeepCopyDecl(name) \
    void deepCopy(const name##Value& src, name##Value& target);
buildForAllTypes(makeDeepCopyDecl, )
#undef makeDeepCopyDecl

    template <typename ValueType>
    std::shared_ptr<ValueType> deepCopy(const ValueType& src) {
    auto target = construct<ValueType>();
    deepCopy(src, *target);
    return target;
}
#endif /* SRC_TYPES_FORWARDDECLS_COPY_H_ */
