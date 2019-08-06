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
    os << "}";
    return os;
}

template <>
ostream& prettyPrint<SetView>(ostream& os, const SetDomain& domain,
                              const SetView& v) {
    os << "{";
    mpark::visit(
        [&](auto& membersImpl) {
            typedef
                typename AssociatedValueType<viewType(membersImpl)>::type Value;
            typedef typename AssociatedDomain<Value>::type Domain;
            const auto& domainPtr =
                mpark::get<shared_ptr<Domain>>(domain.inner);
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
    os << "}";
    return os;
}

template <typename InnerViewType>
void deepCopyImpl(const SetValue& src,
                  const ExprRefVec<InnerViewType>& srcMemnersImpl,
                  SetValue& target) {
    auto& targetMembersImpl = target.getMembers<InnerViewType>();
    // to be optimised later
    // cannot just clear vector as other constraints will hold pointers to
    // values that are in this set and assume that they are still in this set
    // instead, we remove values that are not   to be in the final target
    size_t index = 0;
    while (index < targetMembersImpl.size()) {
        if (!src.containsMember(targetMembersImpl[index]->view().get())) {
            target.removeMember<
                typename AssociatedValueType<InnerViewType>::type>(index);
        } else {
            ++index;
        }
    }
    for (auto& member : srcMemnersImpl) {
        if (!target.memberHashes.count(getValueHash(
                member->view().checkedGet(NO_SET_UNDEFINED_MEMBERS)))) {
            target.addMember(deepCopy(*assumeAsValue(member)));
        }
    }
    debug_code(target.assertValidState());
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
    mpark::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
        },
        src.members);
}

void matchInnerType(const SetDomain& domain, SetValue& target) {
    mpark::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<decltype(
                    innerDomainImpl)>::element_type>::type>::type>();
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
    mpark::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValue<SetView>(const SetView& u, const SetView& v);
template <>
bool largerValue<SetView>(const SetView& u, const SetView& v);

template <>
bool smallerValue<SetView>(const SetView& u, const SetView& v) {
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
bool largerValue<SetView>(const SetView& u, const SetView& v) {
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
void SetView::assertValidState() {
    mpark::visit(
        [&](auto& valMembersImpl) {
            HashSet<HashType> seenHashes;
            bool success = true;
            HashType calculatedTotal(0);
            if (memberHashes.size() != valMembersImpl.size()) {
                cerr << "memberHashes and members differ in size.\n";
                success = false;
            } else {
                for (size_t i = 0; i < valMembersImpl.size(); i++) {
                    auto& member = valMembersImpl[i];
                    HashType memberHash = getValueHash(
                        member->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
                    if (!seenHashes.insert(memberHash).second) {
                        cerr << "Error: possible duplicate member: "
                             << member->view() << endl;
                        success = false;
                        break;
                    }
                    if (!(memberHashes.count(memberHash))) {
                        cerr << "Error: member " << member->view()
                             << " has no corresponding hash in "
                                "memberHashes.\n";
                        success = false;
                        break;
                    }
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
            }
            if (!success) {
                cerr << "Members: " << valMembersImpl << endl;
                cerr << "memberHashes: " << memberHashes << endl;
                assert(false);
            }
        },
        members);
}

void SetValue::assertValidVarBases() {
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
                         << "'s container does not point to this set." << endl;
                } else if (base.id != i) {
                    success = false;
                    cerr << "member " << i << "has id " << base.id
                         << " but it should be " << i << endl;
                }
            }
            if (!success) {
                cerr << "Members: " << valMembersImpl << endl;
                cerr << "memberHashes: " << memberHashes << endl;
                this->printVarBases();
                assert(false);
            }
        },
        members);
}

void SetValue::printVarBases() {
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

void SetView::standardSanityChecksForThisType() const {
    HashType checkCachedHashTotal(0);
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                auto viewOption = member->getViewIfDefined();
                sanityCheck(viewOption, NO_SET_UNDEFINED_MEMBERS);
                auto& view = *viewOption;
                HashType hash = getValueHash(view);
                checkCachedHashTotal += mix(hash);
                sanityCheck(memberHashes.count(hash),
                            toString("member ", member->view(), " with hash ",
                                     hash, " is not in memberHashes."));
            }
            sanityEqualsCheck(memberHashes.size(), numberElements());
            sanityEqualsCheck(checkCachedHashTotal, cachedHashTotal);
        },
        members);
}

void SetValue::debugSanityCheckImpl() const {
    mpark::visit(
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
