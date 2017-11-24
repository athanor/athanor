
#ifndef SRC_TYPES_FORWARDDECLS_HASH_H_
#define SRC_TYPES_FORWARDDECLS_HASH_H_
#include "types/base.h"
#define makeGetHashDecl(valueName) \
    u_int64_t getValueHash(const valueName##Value&);
buildForAllTypes(makeGetHashDecl, )
#undef makeGetHashDecl

    inline u_int64_t getValueHash(const AnyValRef& val) {
    return mpark::visit(
        [&](const auto& valImpl) { return getValueHash(*valImpl); }, val);
}
#endif /* SRC_TYPES_FORWARDDECLS_HASH_H_ */
