#include <algorithm>
#include <cassert>

#include "common/common.h"
#include "types/setVal.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <>
HashType getValueHash<SetView>(const SetView& val) {
    return val.cachedHashTotal;
}

template <>
ostream& prettyPrint<SetView>(ostream& os, const SetView& v) {
    os << "{";
    lib::visit(
        [&](auto& membersImpl) {
            bool first = true;
            for (auto& memberPtr : membersImpl) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                prettyPrint(os, memberPtr->view());
            }
        },
        v.members);
    os << "}";
    return os;
}

template <>
ostream& prettyPrint<SetView>(ostream& os, const SetDomain& domain,
                              const SetView& v) {
    os << ((domain.isRelation) ? "relation (" : "{");
    lib::visit(
        [&](auto& membersImpl) {
            typedef
                typename AssociatedValueType<viewType(membersImpl)>::type Value;
            typedef typename AssociatedDomain<Value>::type Domain;
            const auto& domainPtr = lib::get<shared_ptr<Domain>>(domain.inner);
            bool first = true;
            for (auto& memberPtr : membersImpl) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                prettyPrint(os, *domainPtr, memberPtr->view());
            }
        },
        v.members);
    os << ((domain.isRelation) ? ")" : "}");
    return os;
}

template <typename InnerViewType>
void deepCopyImpl(const SetValue&,
                  const ExprRefVec<InnerViewType>& srcMemnersImpl,
                  SetValue& target) {
    target.silentClear();
    for (auto& member : srcMemnersImpl) {
        target.addMember(deepCopy(*assumeAsValue(member)));
    }
    debug_code(target.standardSanityChecksForThisType());
    target.notifyEntireValueChanged();
}

template <>
void deepCopy<SetValue>(const SetValue& src, SetValue& target) {
    assert(src.members.index() == target.members.index());
    return visit(
        [&](auto& srcMembersImpl) {
            return deepCopyImpl(src, srcMembersImpl, target);
        },
        src.members);
}

template <>
ostream& prettyPrint<SetDomain>(ostream& os, const SetDomain& d) {
    os << "set(";
    os << d.sizeAttr << ",";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

void matchInnerType(const SetValue& src, SetValue& target) {
    lib::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
        },
        src.members);
}

void matchInnerType(const SetDomain& domain, SetValue& target) {
    lib::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<
                    decltype(innerDomainImpl)>::element_type>::type>::type>();
        },
        domain.inner);
}

template <>
UInt getDomainSize<SetDomain>(const SetDomain& domain) {
    UInt innerDomainSize = getDomainSize(domain.inner);
    if (innerDomainSize > (sizeof(UInt) * 4) - 1) {
        return MAX_DOMAIN_SIZE;
    } else {
        return 1 << innerDomainSize;
    }
}

template <typename InnerViewType>
void normaliseImpl(SetValue&, ExprRefVec<InnerViewType>& valMembersImpl) {
    for (auto& v : valMembersImpl) {
        normalise(*assumeAsValue(v));
    }
    sort(valMembersImpl.begin(), valMembersImpl.end(),
         [](auto& u, auto& v) { return smallerValue(u->view(), v->view()); });
}

template <>
void normalise<SetValue>(SetValue& val) {
    lib::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValueImpl<SetView>(const SetView& u, const SetView& v);
template <>
bool largerValueImpl<SetView>(const SetView& u, const SetView& v);

template <>
bool smallerValueImpl<SetView>(const SetView& u, const SetView& v) {
    todoImpl(u, v);
}

template <>
bool largerValueImpl<SetView>(const SetView& u, const SetView& v) {
    todoImpl(u, v);
}

template <>
bool equalValueImpl<SetView>(const SetView& u, const SetView& v) {
    return lib::visit(
        [&](const auto& uMembersImpl) {
            auto& vMembersImpl =
                lib::get<BaseType<decltype(uMembersImpl)>>(v.members);
            if (uMembersImpl.size() != vMembersImpl.size()) {
                return false;
            }
            for (auto& hashIndexPair : u.hashIndexMap) {
                auto iter = v.hashIndexMap.find(hashIndexPair.first);
                if (iter == v.hashIndexMap.end()) {
                    return false;
                }
                auto& uMember = uMembersImpl[hashIndexPair.second];
                auto& vMember = vMembersImpl[iter->second];
                if (!equalValue(uMember->getViewIfDefined(),
                                vMember->getViewIfDefined())) {
                    return false;
                }
            }
            return true;
        },
        u.members);
}

void SetValue::assertValidVarBases() {
    lib::visit([&](auto& members) { varBaseSanityChecks(*this, members); },
               members);
}

void SetValue::printVarBases() {
    lib::visit(
        [&](auto& valMembersImpl) {
            cout << "parent is constant: " << (this->container == &constantPool)
                 << endl;
            for (auto& member : valMembersImpl) {
                cout << "val id: " << valBase(*assumeAsValue(member)).id
                     << endl;
                cout << "is constant: "
                     << (valBase(*assumeAsValue(member)).container ==
                         &constantPool)
                     << endl;
            }
        },
        members);
}

void SetView::standardSanityChecksForThisType() const {
    HashType checkCachedHashTotal(0);
    lib::visit(
        [&](auto& members) {
            sanityEqualsCheck(members.size(), indexHashMap.size());
            for (size_t index = 0; index < members.size(); index++) {
                auto& member = members[index];
                auto viewOption = member->getViewIfDefined();
                sanityCheck(viewOption, NO_SET_UNDEFINED_MEMBERS);
                auto& view = *viewOption;
                HashType hash = getValueHash(view);
                checkCachedHashTotal += mix(hash);
                sanityCheck(hashIndexMap.count(hash),
                            toString("member with index ", index, " and value ",
                                     member->view(), " with hash ", hash,
                                     " is not in hashIndexMap."));
                sanityEqualsCheck(index, hashIndexMap.at(hash));
                sanityEqualsCheck(hash, indexHashMap[index]);
            }
            sanityEqualsCheck(hashIndexMap.size(), numberElements());
            sanityEqualsCheck(checkCachedHashTotal, cachedHashTotal);
        },
        members);
}

void SetValue::debugSanityCheckImpl() const {
    lib::visit(
        [&](auto& members) {
            recurseSanityChecks(members);
            standardSanityChecksForThisType();
            varBaseSanityChecks(*this, members);
        },
        members);
}

template <>
bool hasVariableSize<SetValue>(const SetValue&) {
    return true;
}
template <>
UInt getSize<SetValue>(const SetValue& v) {
    return v.numberElements();
}

AnyExprVec& SetValue::getChildrenOperands() { return members; }

template <>
size_t getResourceLowerBound<SetDomain>(const SetDomain& domain) {
    return domain.sizeAttr.minSize * getResourceLowerBound(domain.inner) + 1;
}
