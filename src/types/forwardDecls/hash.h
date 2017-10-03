
#ifndef SRC_TYPES_FORWARDDECLS_HASH_H_
#define SRC_TYPES_FORWARDDECLS_HASH_H_
#include "types/forwardDecls/typesAndDomains.h"
#define makeGetHashDecl(valueName) \
    u_int64_t getValueHash(const valueName##Value&);
buildForAllTypes(makeGetHashDecl, )
#undef makeGetHashDecl

#endif /* SRC_TYPES_FORWARDDECLS_HASH_H_ */
