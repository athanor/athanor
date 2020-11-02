#include <algorithm>
#include <cassert>

#include "common/common.h"
#include "types/mSetVal.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <>
HashType getValueHash<MSetView>(const MSetView& val) {
    return val.cachedHashTotal;
}

template <>
ostream& prettyPrint<MSetView>(ostream& os, const MSetView& v) {
    os << "mset(";
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
    os << ")";
    return os;
}

template <>
ostream& prettyPrint<MSetView>(ostream& os, const MSetDomain& domain,
                               const MSetView& v) {
    os << "mset(";
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
    os << ")";
    return os;
}

template <typename InnerViewType>
void deepCopyImpl(const MSetValue&,
                  const ExprRefVec<InnerViewType>& srcMemnersImpl,
                  MSetValue& target) {
    // to be optimised later
    target.silentClear();

    for (auto& member : srcMemnersImpl) {
        target.addMember(deepCopy(*assumeAsValue(member)));
    }
    debug_code(target.debugSanityCheck());
    target.notifyEntireValueChanged();
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
    os << "mSet(";
    os << d.sizeAttr << ",";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

void matchInnerType(const MSetValue& src, MSetValue& target) {
    lib::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
        },
        src.members);
}

void matchInnerType(const MSetDomain& domain, MSetValue& target) {
    lib::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<decltype(
                    innerDomainImpl)>::element_type>::type>::type>();
        },
        domain.inner);
}

template <>
UInt getDomainSize<MSetDomain>(const MSetDomain&) {
    return MAX_DOMAIN_SIZE;
    //    todoImpl(domain);
}

void evaluateImpl(MSetValue&) {}
void startTriggeringImpl(MSetValue&) {}
void stopTriggering(MSetValue&) {}

template <typename InnerViewType>
void normaliseImpl(MSetValue&, ExprRefVec<InnerViewType>& valMembersImpl) {
    for (auto& v : valMembersImpl) {
        normalise(*assumeAsValue(v));
    }
    sort(valMembersImpl.begin(), valMembersImpl.end(),
         [](auto& u, auto& v) { return smallerValue(u->view(), v->view()); });
}

template <>
void normalise<MSetValue>(MSetValue& val) {
    lib::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValueImpl<MSetView>(const MSetView& u, const MSetView& v);
template <>
bool largerValueImpl<MSetView>(const MSetView& u, const MSetView& v);
template <>
bool equalValueImpl<MSetView>(const MSetView& u, const MSetView& v);

template <>
bool smallerValueImpl<MSetView>(const MSetView& u, const MSetView& v) {
    todoImpl(u, v);
}

template <>
bool largerValueImpl<MSetView>(const MSetView& u, const MSetView& v) {
    todoImpl(u, v);
}

template <>
bool equalValueImpl<MSetView>(const MSetView& u, const MSetView& v) {
    todoImpl(u, v);
}

void MSetValue::printVarBases() {
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

void MSetView::standardSanityChecksForThisType() const {
    HashType checkCachedHashTotal(0);
    HashMap<HashType, UInt> checkMemberCounts;
    lib::visit(
        [&](auto& members) {
            for (size_t i = 0; i < members.size(); i++) {
                auto& member = members[i];
                auto viewOption = member->getViewIfDefined();
                sanityCheck(viewOption, NO_MSET_UNDEFINED_MEMBERS);
                auto& view = *viewOption;
                sanityCheck(i < indexHashMap.size(),
                            "indexHashMap has wrong size.");
                HashType hash = getValueHash(view);
                sanityEqualsCheck(hash, indexHashMap[i]);
                checkCachedHashTotal += mix(hash);
                ++checkMemberCounts[hash];
            }
            sanityEqualsCheck(checkCachedHashTotal, cachedHashTotal);
            sanityEqualsCheck(memberCounts, checkMemberCounts);
            sanityEqualsCheck(members.size(), indexHashMap.size());
        },
        members);
}

void MSetValue::debugSanityCheckImpl() const {
    lib::visit(
        [&](auto& members) {
            recurseSanityChecks(members);
            standardSanityChecksForThisType();
            varBaseSanityChecks(*this, members);
        },
        members);
}

template <>
bool hasVariableSize<MSetValue>(const MSetValue&) {
    return true;
}
template <>
UInt getSize<MSetValue>(const MSetValue& v) {
    return v.numberElements();
}

AnyExprVec& MSetValue::getChildrenOperands() { return members; }

template <>
size_t getResourceLowerBound<MSetDomain>(const MSetDomain& domain) {
    return domain.sizeAttr.minSize * getResourceLowerBound(domain.inner) + 1;
}
