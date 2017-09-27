#include "types/set.h"
#include <cassert>
#include "common/common.h"
#include "types/parentCheck.h"
template <typename InnerType>
void assignInitialValueInDomainImpl(SetValueImpl<InnerType>& val,
                                    const SetDomain& domain) {
    val.members.assign(domain.sizeAttr.minSize, InnerType());
    assert(false);
    abort();
}
void assignInitialValueInDomain(SetValue& val, const SetDomain& domain) {
    mpark::visit([&](auto& v) { assignInitialValueInDomainImpl(v, domain); },
                 val.setValueImpl);
}

template <typename InnerType>
bool moveToNextValueInDomainImpl(SetValueImpl<InnerType>& val,
                                 const SetDomain& domain,
                                 const ParentCheck& parentCheck) {
    assert(false);
    abort();
}

bool moveToNextValueInDomain(SetValue& val, const SetDomain& domain,
                             const ParentCheck& parentCheck) {
    return mpark::visit(
        [&](auto& v) {
            return moveToNextValueInDomainImpl(v, domain, parentCheck);
        },
        val.setValueImpl);
}

template <typename InnerType>
u_int64_t getHashImpl(const SetValueImpl<InnerType>&) {
    assert(false);
    abort();
}
u_int64_t getHash(const SetValue& val) {
    return mpark::visit([&](auto& v) { return getHashImpl(v); },
                        val.setValueImpl);
}
