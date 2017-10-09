#include "types/set.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "types/forwardDecls/constructValue.h"
#include "types/forwardDecls/copy.h"
#include "types/forwardDecls/getDomainSize.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
using namespace std;

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
        mpark::get<SetValueImpl<InnerValuePtrType>>(target.setValueImpl);
    targetImpl.memberHashes = srcImpl.memberHashes;
    targetImpl.cachedHashTotal = srcImpl.cachedHashTotal;
    if (srcImpl.members.size() < targetImpl.members.size()) {
        targetImpl.members.erase(
            targetImpl.members.begin() + srcImpl.members.size(),
            targetImpl.members.end());
    }
    while (srcImpl.members.size() > targetImpl.members.size()) {
        auto member = construct<typename InnerValuePtrType::element_type>();
        matchInnerType(*srcImpl.members[0], *member);
        targetImpl.members.push_back(std::move(member));
    }

    transform(srcImpl.members.begin(), srcImpl.members.end(),
              targetImpl.members.begin(),
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

template <typename InnerValuePtrType>
void matchInnerTypeImpl(const SetValueImpl<InnerValuePtrType>&,
                        SetValue& target) {
    if (mpark::get_if<SetValueImpl<InnerValuePtrType>>(
            &(target.setValueImpl)) == NULL) {
        target.setValueImpl = SetValueImpl<InnerValuePtrType>();
    }
}

void matchInnerType(const SetValue& src, SetValue& target) {
    mpark::visit([&](auto& srcImpl) { matchInnerTypeImpl(srcImpl, target); },
                 src.setValueImpl);
}

template <typename InnerDomainPtrType>
void matchInnerTypeFromDomain(const InnerDomainPtrType&, SetValue& target) {
    typedef shared_ptr<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValuePtrType;
    if (mpark::get_if<SetValueImpl<InnerValuePtrType>>(
            &(target.setValueImpl)) == NULL) {
        target.setValueImpl = SetValueImpl<InnerValuePtrType>();
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
