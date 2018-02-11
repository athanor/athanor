#include "types/mSet.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "operators/operatorBase.h"
#include "types/typeOperations.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <>
u_int64_t getValueHash<MSetValue>(const MSetValue& val) {
    todoImpl(val);
}

template <>
ostream& prettyPrint<MSetValue>(ostream& os, const MSetValue& v) {
    os << "MSet({";
    mpark::visit(
        [&](auto& membersImpl) {
            bool first = true;
            for (auto& memberPtr : membersImpl) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                prettyPrint(os, *memberPtr);
            }
        },
        v.members);
    os << "})";
    return os;
}

template <typename InnerValueType>
void deepCopyImpl(const MSetValue& src,
                  const ValRefVec<InnerValueType>& srcMemnersImpl,
                  MSetValue& target) {
    todoImpl(src, srcMemnersImpl, target);
}

template <>
void deepCopy<MSetValue>(const MSetValue& src, MSetValue& target) {
    assert(src.members.index() == target.members.index());
    return visit(
        [&](auto& srcMembersImpl) {
            return deepCopyImpl(src, srcMembersImpl, target);
        },
        src.members);
}

template <>
ostream& prettyPrint<MSetDomain>(ostream& os, const MSetDomain& d) {
    os << "MSet(";
    os << d.sizeAttr << ",";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

template <typename InnerValueType>
void matchInnerTypeImpl(const ValRefVec<InnerValueType>&, MSetValue& target) {
    if (mpark::get_if<ValRefVec<InnerValueType>>(&(target.members)) == NULL) {
        target.members.emplace<ValRefVec<InnerValueType>>();
    }
}

void matchInnerType(const MSetValue& src, MSetValue& target) {
    mpark::visit(
        [&](auto& srcMembersImpl) {
            matchInnerTypeImpl(srcMembersImpl, target);
        },
        src.members);
}

template <typename InnerDomainType>
void matchInnerTypeFromDomain(const std::shared_ptr<InnerDomainType>&,
                              MSetValue& target) {
    typedef typename AssociatedValueType<InnerDomainType>::type InnerValueType;
    if (mpark::get_if<ValRefVec<InnerValueType>>(&(target.members)) == NULL) {
        target.members.emplace<ValRefVec<InnerValueType>>();
    }
}

void matchInnerType(const MSetDomain& domain, MSetValue& target) {
    mpark::visit(
        [&](auto& innerDomainImpl) {
            matchInnerTypeFromDomain(innerDomainImpl, target);
        },
        domain.inner);
}

template <>
u_int64_t getDomainSize<MSetDomain>(const MSetDomain& domain) {
    todoImpl(domain);
}

void reset(MSetValue& val) {
    val.container = NULL;
    val.triggers.clear();
    mpark::visit([&](auto& members) { members.clear(); }, val.members);
}

void evaluate(MSetValue&) {}
void startTriggering(MSetValue&) {}
void stopTriggering(MSetValue&) {}

template <typename InnerValueType>
void normaliseImpl(MSetValue& val, ValRefVec<InnerValueType>& valMembersImpl) {
    ignoreUnused(val);
    for (auto& v : valMembersImpl) {
        normalise(*v);
    }
    sort(valMembersImpl.begin(), valMembersImpl.end(),
         [](auto& u, auto& v) { return smallerValue(*u, *v); });
}

template <>
void normalise<MSetValue>(MSetValue& val) {
    mpark::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValue<MSetValue>(const MSetValue& u, const MSetValue& v);
template <>
bool largerValue<MSetValue>(const MSetValue& u, const MSetValue& v);

template <>
bool smallerValue<MSetValue>(const MSetValue& u, const MSetValue& v) {
    return mpark::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                mpark::get<BaseType<decltype(uMembersImpl)>>(v.members);
            if (uMembersImpl.size() < vMembersImpl.size()) {
                return true;
            } else if (uMembersImpl.size() > vMembersImpl.size()) {
                return false;
            }
            for (size_t i = 0; i < uMembersImpl.size(); ++i) {
                if (smallerValue(*uMembersImpl[i], *vMembersImpl[i])) {
                    return true;
                } else if (largerValue(*uMembersImpl[i], *vMembersImpl[i])) {
                    return false;
                }
            }
            return false;
        },
        u.members);
}

template <>
bool largerValue<MSetValue>(const MSetValue& u, const MSetValue& v) {
    return mpark::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                mpark::get<BaseType<decltype(uMembersImpl)>>(v.members);
            if (uMembersImpl.size() > vMembersImpl.size()) {
                return true;
            } else if (uMembersImpl.size() < vMembersImpl.size()) {
                return false;
            }
            for (size_t i = 0; i < uMembersImpl.size(); ++i) {
                if (largerValue(*uMembersImpl[i], *vMembersImpl[i])) {
                    return true;
                } else if (smallerValue(*uMembersImpl[i], *vMembersImpl[i])) {
                    return false;
                }
            }
            return false;
        },
        u.members);
}
