
#ifndef SRC_BASE_VALBASE_H_
#define SRC_BASE_VALBASE_H_
#include "base/refs.h"
struct ValBase;
extern ValBase constantPool;

struct ValBase {
    u_int64_t id = 0;
    ValBase* container = &constantPool;
};

template <typename Val>
ValBase& valBase(Val& val);
template <typename Val>
const ValBase& valBase(const Val& val);

template <typename T = int>
ValBase& valBase(const AnyValRef& ref, T = 0) {
    return mpark::visit([](auto& val) { return valBase(val); }, ref);
}

#endif /* SRC_BASE_VALBASE_H_ */
