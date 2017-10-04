#include "types/set.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/copy.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename DomainPtrType>
inline auto& getAssociatedValue(DomainPtrType&, SetValueImplWrapper& value) {
    return mpark::get<SetValueImpl<shared_ptr<typename AssociatedValueType<
        typename DomainPtrType::element_type>::type>>>(value);
}

template <typename InnerDomainPtrType,
          typename InnerValuePtrType = typename AssociatedValueType<
              typename InnerDomainPtrType::type>::type>
bool assignIncreasingValues(const InnerDomainPtrType& innerDomain,
                            const size_t startIndex, const size_t endIndex,
                            SetValue& val,
                            SetValueImpl<InnerValuePtrType>& valImpl) {
    for (size_t i = startIndex; i < endIndex; ++i) {
        bool success;
        valImpl.changeMemberValue(
            [&]() {
                deepCopy(*valImpl.members[i - 1], *valImpl.members[i]);
                success = moveToNextValueInDomain(
                    *valImpl.members[i], innerDomain, [&]() {
                        return !valImpl.containsMember(valImpl.members[i]);
                    });
            },
            val, i);
        if (!success) {
            return false;
        }
    }
    return true;
}

template <typename InnerDomainPtrType, typename InnerValuePtrDeducedType,
          typename InnerValuePtrType =
              typename remove_reference<InnerValuePtrDeducedType>::type>
bool assignIncreasingValues(const InnerDomainPtrType& innerDomain,
                            const size_t startIndex, const size_t endIndex,
                            InnerValuePtrDeducedType&& startingValue,
                            SetValue& val,
                            SetValueImpl<InnerValuePtrType>& valImpl) {
    if (startIndex >= valImpl.members.size()) {
        return true;
    }
    valImpl.changeMemberValue(
        [&]() {
            valImpl.members[startIndex] =
                forward<InnerValuePtrDeducedType>(startingValue);
        },
        val, startIndex);
    return assignIncreasingValues(innerDomain, startIndex + 1, endIndex, val,
                                  valImpl);
}

template <typename InnerDomainPtrType,
          typename InnerValuePtrType = shared_ptr<typename AssociatedValueType<
              typename InnerDomainPtrType::element_type>::type>>
void assignInitialValueInDomainImpl(const SetDomain& domain,
                                    const InnerDomainPtrType& innerDomainPtr,
                                    SetValue& val,
                                    SetValueImpl<InnerValuePtrType>& valImpl) {
    // remove values if set larger than minSize
    while (valImpl.members.size() > domain.sizeAttr.minSize) {
        valImpl.removeValue(val, valImpl.members.size() - 1);
    }
    // if set does not have enough elements for minSize
    valImpl.members.reserve(domain.sizeAttr.minSize);
    while (valImpl.members.size() < domain.sizeAttr.minSize) {
        valImpl.addValue(val,
                         construct<typename InnerValuePtrType::element_type>());
    }
    bool success = assignIncreasingValues(
        *innerDomainPtr, 0, valImpl.members.size(),
        makeInitialValueInDomain(*innerDomainPtr), val, valImpl);
    if (!success) {
        cerr << "Error: could not assign initial value.  Trying to print "
                "domain and value...\nDomain: ";
        prettyPrint(cerr, domain) << "\nValue: ";
        prettyPrint(cerr, val) << endl;
        assert(false);
    }
}

void assignInitialValueInDomain(const SetDomain& domain, SetValue& val) {
    mpark::visit(
        [&](auto& domainImpl) {
            assignInitialValueInDomainImpl(
                domain, domainImpl, val,
                getAssociatedValue(domainImpl, val.setValueImpl));
        },
        domain.inner);
    val.state = ValueState::DIRTY;
}

template <typename InnerDomainPtrType,
          typename InnerValuePtrType = shared_ptr<typename AssociatedValueType<
              typename InnerDomainPtrType::element_type>::type>>

shared_ptr<SetValue> makeInitialValueInDomainImpl(
    const SetDomain& domain, const InnerDomainPtrType& innerDomainPtr,
    std::shared_ptr<SetValue>& val) {
    auto& valImpl =
        val->setValueImpl.emplace<SetValueImpl<InnerValuePtrType>>();
    assignInitialValueInDomainImpl(domain, innerDomainPtr, *val, valImpl);
    val->state = ValueState::DIRTY;
    return val;
}

shared_ptr<SetValue> makeInitialValueInDomain(const SetDomain& domain) {
    auto val = construct<SetValue>();
    return mpark::visit(
        [&](auto& innerDomainImpl) {
            return makeInitialValueInDomainImpl(domain, innerDomainImpl, val);
        },
        domain.inner);
}

template <typename InnerValuePtrType>
bool moveToNextValueInDomainImpl(SetValue& val,
                                 SetValueImpl<InnerValuePtrType>& valImpl,
                                 const SetDomain& domain,
                                 const ParentCheck& parentCheck) {
    typedef shared_ptr<typename AssociatedDomain<
        typename InnerValuePtrType::element_type>::type>
        InnerDomainPtrType;
    auto& innerDomainPtr = mpark::get<InnerDomainPtrType>(domain.inner);
    while (true) {
        size_t index = valImpl.members.size();
        while (index > 0) {
            bool successfulChange;
            valImpl.changeMemberValue(
                [&]() {
                    successfulChange = moveToNextValueInDomain(
                        *valImpl.members[index - 1], *innerDomainPtr, [&]() {
                            return !valImpl.containsMember(
                                valImpl.members[index - 1]);
                        });
                },
                val, index - 1);
            if (successfulChange &&
                (index == valImpl.members.size() ||
                 assignIncreasingValues(*innerDomainPtr, index,
                                        valImpl.members.size(), val,
                                        valImpl))) {
                if (parentCheck()) {
                    val.state = ValueState::DIRTY;
                    return true;
                } else {
                    index = valImpl.members.size();
                    continue;
                }
            }
            --index;
        }
        if (valImpl.members.size() >= domain.sizeAttr.maxSize) {
            val.state = ValueState::UNDEFINED;
            return false;  // inner domain size limit reached
        } else {
            valImpl.addValue(
                val, construct<typename InnerValuePtrType::element_type>());
            bool success = assignIncreasingValues(
                *innerDomainPtr, 0, valImpl.members.size(),
                makeInitialValueInDomain(*innerDomainPtr), val, valImpl);
                        ;
            if (!success) {
                val.state = ValueState::UNDEFINED;
                return false;  // inner domain size limit reached
            }
            if (parentCheck()) {
                val.state = ValueState::DIRTY;
                return true;
            } else {
                index = valImpl.members.size();
                continue;
            }
        }
    }
}

bool moveToNextValueInDomain(SetValue& val, const SetDomain& domain,
                             const ParentCheck& parentCheck) {
    return mpark::visit(
        [&](auto& valImpl) {
            return moveToNextValueInDomainImpl(val, valImpl, domain,
                                               parentCheck);
        },
        val.setValueImpl);
}

u_int64_t getValueHash(const SetValue& val) {
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

ostream& prettyPrint(ostream& os, const SetDomain& d) {
    os << "set(";
    os << d.sizeAttr << ",";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

vector<shared_ptr<SetTrigger>>& getTriggers(SetValue& v) { return v.triggers; }

SetView getSetView(SetValue& val) {
    return mpark::visit(
        [&](auto& valImpl) {
            return SetView(valImpl.memberHashes, valImpl.cachedHashTotal,
                           val.triggers);
        },
        val.setValueImpl);
}
