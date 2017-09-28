#include "types/set.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/copy.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "types/parentCheck.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename InnerDomainPtrType>
shared_ptr<SetValue> makeInitialValueInDomainImpl(
    const SetDomain& domain, const InnerDomainPtrType& innerDomainPtr) {
    typedef shared_ptr<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValuePtrType;
    auto val = make_shared<SetValue>();
    auto& valImpl =
        val->setValueImpl.emplace<SetValueImpl<InnerValuePtrType>>();
    valImpl.members.reserve(domain.sizeAttr.minSize);
    for (size_t i = 0; i < domain.sizeAttr.minSize; ++i) {
        if (i == 0) {
            valImpl.members.push_back(
                makeInitialValueInDomain(*innerDomainPtr));
        } else {
            valImpl.members.push_back(deepCopy(*valImpl.members[i - 1]));
            moveToNextValueInDomain(
                *valImpl.members.back(), *innerDomainPtr, [&]() {
                    return !valImpl.memberHashes.count(
                        mix(getHash(*valImpl.members.back())));
                });
        }
        u_int64_t hash = mix(getHash(*valImpl.members.back()));
        valImpl.memberHashes.insert(hash);
        valImpl.cachedHashTotal += hash;
    }
    return val;
}

shared_ptr<SetValue> makeInitialValueInDomain(const SetDomain& domain) {
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

ostream& prettyPrint(ostream& os, const SetValue& v) {
    os << "set({";
    mpark::visit(
        [&](auto& vImpl) {
            bool first = true;
            for (auto& memberPtr : vImpl.members) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                prettyPrint(os, *memberPtr);
                ;
            }
        },
        v.setValueImpl);
    os << "})";
    return os;
}

template <typename InnerValuePtrType>
shared_ptr<SetValue> deepCopyImpl(
    const SetValueImpl<InnerValuePtrType>& origImpl) {
    auto newValue = make_shared<SetValue>();
    auto& newValueImpl =
        newValue->setValueImpl.emplace<SetValueImpl<InnerValuePtrType>>();
    newValueImpl.memberHashes = origImpl.memberHashes;
    newValueImpl.cachedHashTotal = origImpl.cachedHashTotal;
    newValueImpl.members.reserve(origImpl.members.size());
    transform(origImpl.members.begin(), origImpl.members.end(),
              back_inserter(newValueImpl.members),
              [](auto& origMemberPtr) { return deepCopy(*origMemberPtr); });
    return newValue;
}

shared_ptr<SetValue> deepCopy(const SetValue& origValue) {
    return visit([](auto& origImpl) { return deepCopyImpl(origImpl); },
                 origValue.setValueImpl);
}
