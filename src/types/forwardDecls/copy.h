
#ifndef SRC_TYPES_FORWARDDECLS_COPY_H_
#define SRC_TYPES_FORWARDDECLS_COPY_H_
#include "types/forwardDecls/typesAndDomains.h"
#define makeDeepCopyDecl(name) \
    std::shared_ptr<name##Value> deepCopy(const name##Value&);
buildForAllTypes(makeDeepCopyDecl, )
#undef makeDeepCopyDecl

#endif /* SRC_TYPES_FORWARDDECLS_COPY_H_ */
