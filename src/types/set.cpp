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
void deepCopyImpl(const SetValue& src,
                  const ValRefVec<InnerValueType>& srcMemnersImpl,
                  SetValue& target) {
    auto& targetMembersImpl = target.getMembers<InnerValueType>();
    // to be optimised later
    // cannot just clear vector as other constraints will hold pointers to
    // values that are in this set and assume that they are still in this set
    // instead, we remove values that are not   to be in the final target
    size_t index = 0;
    while (index < targetMembersImpl.size()) {
        if (!src.containsMember(*targetMembersImpl[index])) {
            target.removeMember<InnerValueType>(index);
        } else {
            ++index;
        }
    }
    for (auto& member : srcMemnersImpl) {
        if (!target.getHashIndexMap().count(getValueHash(*member))) {
            target.addMember(deepCopy(*member));
        }
    }
    debug_code(target.assertValidState());
    target.notifyEntireSetChange();
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

template <typename InnerValueType>
void matchInnerTypeImpl(const ValRefVec<InnerValueType>&, SetValue& target) {
    if (mpark::get_if<ValRefVec<InnerValueType>>(&(target.members)) == NULL) {
        target.members.emplace<ValRefVec<InnerValueType>>();
    }
}

void matchInnerType(const SetValue& src, SetValue& target) {
    mpark::visit(
        [&](auto& srcMembersImpl) {
            matchInnerTypeImpl(srcMembersImpl, target);
        },
        src.members);
}

template <typename InnerDomainType>
void matchInnerTypeFromDomain(const std::shared_ptr<InnerDomainType>&,
                              SetValue& target) {
    typedef typename AssociatedValueType<InnerDomainType>::type InnerValueType;
    if (mpark::get_if<ValRefVec<InnerValueType>>(&(target.members)) == NULL) {
        target.members.emplace<ValRefVec<InnerValueType>>();
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

void reset(SetValue& val) {
    val.container = NULL;
    val.silentClear();
    val.triggers.clear();
}

void evaluate(SetValue&) {}
void startTriggering(SetValue&) {}
void stopTriggering(SetValue&) {}

template <typename InnerValueType>
void normaliseImpl(SetValue& val, ValRefVec<InnerValueType>& valMembersImpl) {
    for (auto& v : valMembersImpl) {
        normalise(*v);
    }
    sort(valMembersImpl.begin(), valMembersImpl.end(),
         [](auto& u, auto& v) { return smallerValue(*u, *v); });

    for (size_t i = 0; i < valMembersImpl.size(); i++) {
        auto& member = valMembersImpl[i];
        val.hashIndexMap[getValueHash(*member)] = i;
    }
}

template <>
void normalise<SetValue>(SetValue& val) {
    mpark::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValue<SetValue>(const SetValue& u, const SetValue& v);
template <>
bool largerValue<SetValue>(const SetValue& u, const SetValue& v);

template <>
bool smallerValue<SetValue>(const SetValue& u, const SetValue& v) {
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
bool largerValue<SetValue>(const SetValue& u, const SetValue& v) {
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

void SetView::assertValidState() {
    mpark::visit(
        [&](auto& valMembersImpl) {
            std::unordered_set<u_int64_t> seenHashes;
            bool success = true;
            u_int64_t calculatedTotal = 0;
            if (getHashIndexMap().size() != valMembersImpl.size()) {
                cerr << "getHashIndexMap() and members differ in size.\n";
                success = false;
            } else {
                for (size_t i = 0; i < valMembersImpl.size(); i++) {
                    auto& member = valMembersImpl[i];
                    u_int64_t memberHash = getValueHash(*member);
                    if (!seenHashes.insert(memberHash).second) {
                        cerr << "Error: possible duplicate member: " << *member
                             << endl;
                        success = false;
                        break;
                    }
                    if (!(getHashIndexMap().count(memberHash))) {
                        cerr << "Error: member " << *member
                             << " has no corresponding hash in "
                                "getHashIndexMap().\n";
                        success = false;
                        break;
                    }
                    if (getHashIndexMap().at(memberHash) != i) {
                        cerr << "Error: member " << *member << "  is at index "
                             << i
                             << " but the hashIndexMap says it should be at "
                             << getHashIndexMap().at(memberHash) << endl;
                        success = false;
                        break;
                    }
                    calculatedTotal += mix(memberHash);
                }
                if (success) {
                    success = calculatedTotal == getCachedHashTotal();
                    if (!success) {
                        cerr << "Calculated hash total should be "
                             << calculatedTotal << " but it was actually "
                             << getCachedHashTotal() << endl;
                    }
                }
            }
            if (success) {
                for (size_t i = 0; i < valMembersImpl.size(); i++) {
                    size_t id = valBase(*valMembersImpl[i]).id;
                    if (i != id) {
                        cerr << "Error: found id " << id
                             << " when it should be " << i << endl;
                        cerr << "ids are: ";
                        transform(
                            begin(valMembersImpl), end(valMembersImpl),
                            ostream_iterator<size_t>(cerr, ","),
                            [](auto& member) { return valBase(*member).id; });
                        success = false;
                        break;
                    }
                }
            }
            if (!success) {
                cerr << "Members: " << valMembersImpl << endl;
                cerr << "memberHashes: " << getHashIndexMap() << endl;
                assert(false);
            }
        },
        members);
}
