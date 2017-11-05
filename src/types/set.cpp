#include "types/set.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "types/forwardDecls/compare.h"
#include "types/forwardDecls/copy.h"
#include "types/forwardDecls/getDomainSize.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
using namespace std;

u_int64_t getValueHash(const SetValue& val) { return val.cachedHashTotal; }

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

void deepCopy(const SetValue& src, SetValue& target) {
    assert(src.setValueImpl.index() == target.setValueImpl.index());
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

u_int64_t getDomainSize(const SetDomain& domain) {
    return 1 << getDomainSize(domain.inner);
}

template <typename InnerValueRefType>
void resetImpl(SetValue& val, SetValueImpl<InnerValueRefType>& valImpl) {
    val.triggers.clear();
    valImpl.members.clear();
    val.memberHashes.clear();
    val.cachedHashTotal = 0;
}
void reset(SetValue& val) {
    mpark::visit([&](auto& valImpl) { resetImpl(val, valImpl); },
                 val.setValueImpl);
}

void evaluate(SetValue&) {}
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

void normalise(SetValue& val) {
    mpark::visit([](auto& valImpl) { normaliseImpl(valImpl); },
                 val.setValueImpl);
}

bool smallerValue(const SetValue& u, const SetValue& v) {
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

bool largerValue(const SetValue& u, const SetValue& v) {
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
