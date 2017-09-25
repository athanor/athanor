#ifndef SRC_TYPES_DOMAIN_H_
#define SRC_TYPES_DOMAIN_H_
#include <memory>
#include "variantOperations.h"

struct SetDomain;
struct IntDomain;
using Domain = mpark::variant<SetDomain, IntDomain>;
#endif /* SRC_TYPES_DOMAIN_H_ */
