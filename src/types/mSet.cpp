#include "types/mSet.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <>
HashType getValueHash<MSetView>(const MSetView& val) {
    return val.cachedHashTotal;
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
    target.notifyPossibleMSetValueChange();
    // to be optimised later
    target.silentClear();

    for (auto& member : srcMemnersImpl) {
        target.addMember(deepCopy(*assumeAsValue(member)));
    }
    debug_code(target.assertValidState());
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
    todoImpl(domain);
}

void reset(MSetValue& val) {
    val.container = NULL;
    val.silentClear();
    val.triggers.clear();
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
void MSetView::assertValidState() {
    mpark::visit(
        [&](auto& valMembersImpl) {
            bool success = true;
            UInt calculatedTotal = 0;
            for (size_t i = 0; i < valMembersImpl.size(); i++) {
                auto& member = valMembersImpl[i];
                HashType memberHash = getValueHash(member->view());
                calculatedTotal += mix(memberHash);
            }
            if (success) {
                success = calculatedTotal == cachedHashTotal;
                if (!success) {
                    cerr << "Calculated hash total should be "
                         << calculatedTotal << " but it was actually "
                         << cachedHashTotal << endl;
                }
            }
            if (!success) {
                cerr << "Members: " << valMembersImpl << endl;
                assert(false);
                abort();
            }
        },
        members);
}

void MSetValue::assertValidVarBases() {
    mpark::visit(
        [&](auto& valMembersImpl) {
            if (valMembersImpl.empty()) {
                return;
            }
            bool success = true;
            for (size_t i = 0; i < valMembersImpl.size(); i++) {
                const ValBase& base =
                    valBase(*assumeAsValue(valMembersImpl[i]));
                if (base.container != this) {
                    success = false;
                    cerr << "member " << i
                         << "'s container does not point to this mSet." << endl;
                } else if (base.id != i) {
                    success = false;
                    cerr << "MSet member " << i << "has id " << base.id
                         << " but it should be " << i << endl;
                }
            }
            if (!success) {
                cerr << "Members: " << valMembersImpl << endl;
                this->printVarBases();
                assert(false);
            }
        },
        members);
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
