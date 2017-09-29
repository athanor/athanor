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

/**
 * Assign member of this set at position startingIndex to the given initial
 * value.  Assign all subsequent values in the range startIndex + 1 to endIndex
 * in  a strictly monotonically increasing fashion, that is, a value at position
 * i is obtained by calling moveToNextValueInDomain(...) on the value at
 * position
 * i-1.
 * The hashes for each changed member is updated along with the cachedHashTotal.
 * If there are not enough elements in the domain to fill the range
 * startIndex..endIndex, the values and their associated caches in the range are
 * in an unknown state, they may or may not have been changed.  Note however
 * that the caches are always consistent with the member values along with the
 * cachedHashTotal.
 *
 */
template <typename InnerDomainPtrType, typename InnerValuePtrDeducedType,
          typename InnerValuePtrType =
              typename remove_reference<InnerValuePtrDeducedType>::type>
bool assignIncreasingValues(const InnerDomainPtrType& innerDomain,
                            const size_t startIndex, const size_t endIndex,
                            InnerValuePtrDeducedType&& startingValue,
                            SetValueImpl<InnerValuePtrType>& val) {
    for (size_t i = startIndex; i < endIndex; ++i) {
        if (i == startIndex) {
            val.members[i] = std::forward<InnerValuePtrType>(startingValue);
        } else {
            deepCopy(*val.members[i - 1], *val.members[i]);
            bool success = moveToNextValueInDomain(
                *val.members[i], innerDomain,
                [&]() { return !val.containsMember(val.members[i]); });
            if (!success) {
                return false;
            }
        }
        u_int64_t hash = mix(getHash(*val.members[i]));
        val.memberHashes.insert(hash);
        val.cachedHashTotal += hash;
    }
    return true;
}

template <typename InnerValuePtrType>
void assignInitialValueInDomainImpl(const SetDomain& domain,
                                    SetValueImpl<InnerValuePtrType>& val) {
    typedef shared_ptr<typename AssociatedDomain<
        typename InnerValuePtrType::element_type>::type>
        InnerDomainPtrType;
    auto& innerDomainPtr = mpark::get<InnerDomainPtrType>(domain.inner);
    if (val.members.size() > domain.sizeAttr.minSize) {
        val.members.erase(val.members.begin() + domain.sizeAttr.minSize,
                          val.members.end());
    } else if (val.members.size() < domain.sizeAttr.minSize) {
        val.members.reserve(domain.sizeAttr.minSize);

        while (val.members.size() < domain.sizeAttr.minSize) {
            val.members.push_back(
                construct<typename InnerValuePtrType::element_type>());
        }
    }
    val.cachedHashTotal = 0;
    val.memberHashes.clear();
    bool success =
        assignIncreasingValues(*innerDomainPtr, 0, val.members.size(),
                               makeInitialValueInDomain(*innerDomainPtr), val);
    if (!success) {
        cerr << "Error: could not assign initial value\n";
        assert(false);
    }
}

void assignInitialValueInDomain(const SetDomain& domain, SetValue& val) {
    return mpark::visit(
        [&](auto& valImpl) {
            return assignInitialValueInDomainImpl(domain, valImpl);
        },
        val.setValueImpl);
}

template <typename InnerValuePtrType>
bool moveToNextValueInDomainImpl(SetValueImpl<InnerValuePtrType>& val,
                                 const SetDomain& domain,
                                 const ParentCheck& parentCheck) {
    typedef shared_ptr<typename AssociatedDomain<
        typename InnerValuePtrType::element_type>::type>
        InnerDomainPtrType;
    auto& innerDomainPtr = mpark::get<InnerDomainPtrType>(domain.inner);
    while (true) {
        size_t index = val.members.size();
        while (index > 0) {
            if (moveToNextValueInDomain(
                    *val.members[index - 1], *innerDomainPtr,
                    [&]() {
                        return !val.containsMember(val.members[index - 1]);
                    }) &&
                (index == val.members.size() ||
                 assignIncreasingValues(*innerDomainPtr, index,
                                        val.members.size(),
                                        val.members[index - 1], val))) {
                if (parentCheck()) {
                    return true;
                } else {
                    index = val.members.size();
                    continue;
                }
            }
            --index;
        }
        if (val.members.size() < domain.sizeAttr.maxSize) {
            val.members.push_back(
                construct<typename InnerValuePtrType::element_type>());
            bool success = assignIncreasingValues(
                *innerDomainPtr, 0, val.members.size(),
                makeInitialValueInDomain(*innerDomainPtr), val);
            if (!success) {
                return false;  // inner domain size limit reached
            }
            if (parentCheck()) {
                return true;
            } else {
                index = val.members.size();
                continue;
            }
        }
    }
}

bool moveToNextValueInDomain(SetValue& val, const SetDomain& domain,
                             const ParentCheck& parentCheck) {
    return mpark::visit(
        [&](auto& v) {
            return moveToNextValueInDomainImpl(v, domain, parentCheck);
        },
        val.setValueImpl);
}

u_int64_t getHash(const SetValue& val) {
    return mpark::visit([](auto& valImpl) { return valImpl.cachedHashTotal; },
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
void deepCopyImpl(const SetValueImpl<InnerValuePtrType>& srcImpl,
                  SetValue& target) {
    auto& targetImpl =
        target.setValueImpl.emplace<SetValueImpl<InnerValuePtrType>>();
    targetImpl.memberHashes = srcImpl.memberHashes;
    targetImpl.cachedHashTotal = srcImpl.cachedHashTotal;
    targetImpl.members.reserve(srcImpl.members.size());
    transform(srcImpl.members.begin(), srcImpl.members.end(),
              back_inserter(targetImpl.members),
              [](auto& srcMemberPtr) { return deepCopy(*srcMemberPtr); });
}

void deepCopy(const SetValue& src, SetValue& target) {
    return visit([&](auto& srcImpl) { return deepCopyImpl(srcImpl, target); },
                 src.setValueImpl);
}
