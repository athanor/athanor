#include "types/set.h"
#include <cassert>
#include "common/common.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/hash.h"
#include "types/parentCheck.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
template <typename InnerDomainPtrType>
std::shared_ptr<SetValue> makeInitialValueInDomainImpl(
    const SetDomain& domain, const InnerDomainPtrType& innerDomainPtr) {
    typedef std::shared_ptr<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValuePtrType;
    auto val = std::make_shared<SetValue>();
    val->setValueImpl = SetValueImpl<InnerValuePtrType>();
    auto& valImpl =
        mpark::get<SetValueImpl<InnerValuePtrType>>(val->setValueImpl);
    valImpl.members.reserve(domain.sizeAttr.minSize);
    bool first;
    for (int i = 0; i < domain.sizeAttr.minSize; ++i) {
        valImpl.members.push_back(makeInitialValueInDomain(*innerDomainPtr));
        InnerValuePtrType& memberPtr = valImpl.members.back();
        if (first) {
            first = false;
            u_int64_t hash = mix(getHash(*memberPtr));
            valImpl.memberHashes.insert(hash);
            valImpl.cachedHashTotal += hash;
        } else {
            moveToNextValueInDomain(*memberPtr, *innerDomainPtr, [&]() {
                return !valImpl.memberHashes.count(mix(getHash(*memberPtr)));
            });
            u_int64_t hash = mix(getHash(*memberPtr));
            valImpl.memberHashes.insert(hash);
            valImpl.cachedHashTotal += hash;
        }
    }
    return val;
}

std::shared_ptr<SetValue> makeInitialValueInDomain(const SetDomain& domain) {
    return mpark::visit(
        [&](auto& innerDomPtr) {
            return makeInitialValueInDomainImpl(domain, innerDomPtr);
        },
        domain.inner);
}

template <typename InnerType>
bool moveToNextValueInDomainImpl(SetValueImpl<InnerType>& val,
                                 const SetDomain& domain,
                                 const ParentCheck& parentCheck) {
    ignoreUnused(val, domain, parentCheck);
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
