#include "types/set.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "operators/operatorBase.h"
#include "types/typeOperations.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <>
u_int64_t getValueHash<SetValue>(const SetValue& val) {
    return val.getCachedHashTotal();
}

template <>
ostream& prettyPrint<SetValue>(ostream& os, const SetValue& v) {
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

template <typename InnerValueRefType>
void deepCopyImpl(const SetValueImpl<InnerValueRefType>& srcImpl,
                  SetValue& target) {
    auto& targetImpl =
        mpark::get<SetValueImpl<InnerValueRefType>>(target.setValueImpl);
    // to be optimised later
    targetImpl.removeAllValues(target);
    for (auto& member : srcImpl.members) {
        targetImpl.addValue(target, deepCopy(*member));
    }
}

template <>
void deepCopy<SetValue>(const SetValue& src, SetValue& target) {
    assert(src.setValueImpl.index() == target.setValueImpl.index());
    return visit([&](auto& srcImpl) { return deepCopyImpl(srcImpl, target); },
                 src.setValueImpl);
}

template <>
ostream& prettyPrint<SetDomain>(ostream& os, const SetDomain& d) {
    os << "set(";
    os << d.sizeAttr << ",";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

template <typename InnerValueRefType>
void matchInnerTypeImpl(const SetValueImpl<InnerValueRefType>&,
                        SetValue& target) {
    if (mpark::get_if<SetValueImpl<InnerValueRefType>>(
            &(target.setValueImpl)) == NULL) {
        target.setValueImpl.emplace<SetValueImpl<InnerValueRefType>>();
    }
}

void matchInnerType(const SetValue& src, SetValue& target) {
    mpark::visit([&](auto& srcImpl) { matchInnerTypeImpl(srcImpl, target); },
                 src.setValueImpl);
}

template <typename InnerDomainPtrType>
void matchInnerTypeFromDomain(const InnerDomainPtrType&, SetValue& target) {
    typedef ValRef<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValueRefType;
    if (mpark::get_if<SetValueImpl<InnerValueRefType>>(
            &(target.setValueImpl)) == NULL) {
        target.setValueImpl.emplace<SetValueImpl<InnerValueRefType>>();
    }
}

void matchInnerType(const SetDomain& domain, SetValue& target) {
    mpark::visit(
        [&](auto& innerDomainImpl) {
            matchInnerTypeFromDomain(innerDomainImpl, target);
        },
        domain.inner);
}

template <>
u_int64_t getDomainSize<SetDomain>(const SetDomain& domain) {
    return 1 << getDomainSize(domain.inner);
}

template <typename InnerValueRefType>
void resetImpl(SetValue& val, SetValueImpl<InnerValueRefType>& valImpl) {
    val.triggers.clear();
    val.clear();
    val.container = NULL;
    valImpl.members.clear();
}
void reset(SetValue& val) {
    mpark::visit([&](auto& valImpl) { resetImpl(val, valImpl); },
                 val.setValueImpl);
}

SetMembersVector evaluate(SetValue& val) {
    return mpark::visit(
        [&](auto& valImpl) -> SetMembersVector { return valImpl.members; },
        val.setValueImpl);
}
void startTriggering(SetValue&) {}
void stopTriggering(SetValue&) {}

template <typename InnerValueRefType>
void normaliseImpl(SetValueImpl<InnerValueRefType>& valImpl) {
    for (auto& v : valImpl.members) {
        normalise(*v);
    }
    sort(valImpl.members.begin(), valImpl.members.end(),
         [](auto& u, auto& v) { return smallerValue(*u, *v); });
}

template <>
void normalise<SetValue>(SetValue& val) {
    mpark::visit([](auto& valImpl) { normaliseImpl(valImpl); },
                 val.setValueImpl);
}

template <>
bool smallerValue<SetValue>(const SetValue& u, const SetValue& v);
template <>
bool largerValue<SetValue>(const SetValue& u, const SetValue& v);

template <>
bool smallerValue<SetValue>(const SetValue& u, const SetValue& v) {
    return mpark::visit(
        [&](auto& uImpl) {
            auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v.setValueImpl);
            if (uImpl.members.size() < vImpl.members.size()) {
                return true;
            } else if (uImpl.members.size() > vImpl.members.size()) {
                return false;
            }
            for (size_t i = 0; i < uImpl.members.size(); ++i) {
                if (smallerValue(*uImpl.members[i], *vImpl.members[i])) {
                    return true;
                } else if (largerValue(*uImpl.members[i], *vImpl.members[i])) {
                    return false;
                }
            }
            return false;
        },
        u.setValueImpl);
}

template <>
bool largerValue<SetValue>(const SetValue& u, const SetValue& v) {
    return mpark::visit(
        [&](auto& uImpl) {
            auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v.setValueImpl);
            if (uImpl.members.size() > vImpl.members.size()) {
                return true;
            } else if (uImpl.members.size() < vImpl.members.size()) {
                return false;
            }
            for (size_t i = 0; i < uImpl.members.size(); ++i) {
                if (largerValue(*uImpl.members[i], *vImpl.members[i])) {
                    return true;
                } else if (smallerValue(*uImpl.members[i], *vImpl.members[i])) {
                    return false;
                }
            }
            return false;
        },
        u.setValueImpl);
}
