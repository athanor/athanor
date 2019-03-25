#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "types/mSetVal.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <>
HashType getValueHash<MSetView>(const MSetView& val) {
    return val.cachedHashTotal.getOrSet([&]() {
        return mpark::visit(
            [&](auto& membersImpl) {
                HashType total(0);
                for (auto& member : membersImpl) {
                    total +=
                        mix(getValueHash(member->getViewIfDefined().checkedGet(
                            NO_MSET_UNDEFINED_MEMBERS)));
                }
                return total;
            },
            val.members);
    });
}

template <>
ostream& prettyPrint<MSetView>(ostream& os, const MSetView& v) {
    os << "mset(";
    mpark::visit(
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
    target.notifyEntireMSetChange();
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
    mpark::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
        },
        src.members);
}

void matchInnerType(const MSetDomain& domain, MSetValue& target) {
    mpark::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<decltype(
                    innerDomainImpl)>::element_type>::type>::type>();
        },
        domain.inner);
}

template <>
UInt getDomainSize<MSetDomain>(const MSetDomain& domain) {
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
    mpark::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValue<MSetView>(const MSetView& u, const MSetView& v);
template <>
bool largerValue<MSetView>(const MSetView& u, const MSetView& v);

template <>
bool smallerValue<MSetView>(const MSetView& u, const MSetView& v) {
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
                if (smallerValue(uMembersImpl[i]->view(),
                                 vMembersImpl[i]->view())) {
                    return true;
                } else if (largerValue(uMembersImpl[i]->view(),
                                       vMembersImpl[i]->view())) {
                    return false;
                }
            }
            return false;
        },
        u.members);
}

template <>
bool largerValue<MSetView>(const MSetView& u, const MSetView& v) {
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
                if (largerValue(uMembersImpl[i]->view(),
                                vMembersImpl[i]->view())) {
                    return true;
                } else if (smallerValue(uMembersImpl[i]->view(),
                                        vMembersImpl[i]->view())) {
                    return false;
                }
            }
            return false;
        },
        u.members);
}


void MSetValue::printVarBases() {
    mpark::visit(
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
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                auto viewOption = member->getViewIfDefined();
                sanityCheck(viewOption, NO_MSET_UNDEFINED_MEMBERS);
                auto& view = *viewOption;
                if (cachedHashTotal.isValid()) {
                    HashType hash = getValueHash(view);
                    checkCachedHashTotal += mix(hash);
                }
            }
            cachedHashTotal.applyIfValid([&](const auto& value) {
                sanityEqualsCheck(checkCachedHashTotal, value);
            });
        },
        members);
}

void MSetValue::debugSanityCheckImpl() const {
    mpark::visit(
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
