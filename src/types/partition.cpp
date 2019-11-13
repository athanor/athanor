#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "types/partitionVal.h"
#include "utils/ignoreUnused.h"
using namespace std;

const HashType PartitionView::INITIAL_HASH(0);
template <>
HashType getValueHash<PartitionView>(const PartitionView& val) {
    return val.cachedHashTotal;
}

template <typename InnerViewType>
void printPart(std::ostream& os, const ExprRefVec<InnerViewType>& part) {
    bool first = true;
    os << "{";
    for (auto& member : part) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        prettyPrint(os, member);
    }
    os << "}";
}

template <typename InnerDomainType, typename InnerViewType>
void printPart(std::ostream& os, const InnerDomainType& domain,
               const ExprRefVec<InnerViewType>& part) {
    bool first = true;
    os << "{";
    for (auto& member : part) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        prettyPrint(os, domain, member->view());
    }
    os << "}";
}

template <>
ostream& prettyPrint<PartitionView>(ostream& os, const PartitionView& v) {
    os << "partition(";
    mpark::visit(
        [&](auto& membersImpl) {
            vector<ExprRefVec<viewType(membersImpl)>> parts(v.numberParts());
            for (size_t i = 0; i < v.memberPartMap.size(); i++) {
                parts[v.memberPartMap[i]].emplace_back(membersImpl[i]);
            }
            bool first = true;
            for (auto& part : parts) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                printPart(os, part);
            }
        },
        v.members);
    os << ")";
    return os;
}

template <>
ostream& prettyPrint<PartitionView>(ostream& os, const PartitionDomain& domain,
                                    const PartitionView& v) {
    os << "partition(";
    mpark::visit(
        [&](auto& membersImpl) {
            typedef
                typename AssociatedValueType<viewType(membersImpl)>::type Value;
            typedef typename AssociatedDomain<Value>::type Domain;
            const auto& domainPtr =
                mpark::get<shared_ptr<Domain>>(domain.inner);

            vector<ExprRefVec<viewType(membersImpl)>> parts(v.numberParts());
            for (size_t i = 0; i < v.memberPartMap.size(); i++) {
                parts[v.memberPartMap[i]].emplace_back(membersImpl[i]);
            }
            bool first = true;
            for (auto& part : parts) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                printPart(os, *domainPtr, part);
            }
        },
        v.members);
    os << ")";
    return os;
}

template <typename InnerViewType>
void deepCopyImpl(const PartitionValue& src,
                  const ExprRefVec<InnerViewType>& srcMembersImpl,
                  PartitionValue& target) {
    // to be optimised later
    target.silentClear();

    for (size_t i = 0; i < srcMembersImpl.size(); i++) {
        target.addMember(src.memberPartMap[i],
                         assumeAsValue(srcMembersImpl[i]));
    }
    debug_code(target.debugSanityCheck());
    target.notifyEntireValueChanged();
}

template <>
void deepCopy<PartitionValue>(const PartitionValue& src,
                              PartitionValue& target) {
    assert(src.members.index() == target.members.index());
    return visit(
        [&](auto& srcMembersImpl) {
            return deepCopyImpl(src, srcMembersImpl, target);
        },
        src.members);
}

template <>
ostream& prettyPrint<PartitionDomain>(ostream& os, const PartitionDomain& d) {
    os << "partition(";
    if (d.regular) {
        os << "regular,";
    }
    os << "numberParts=" << d.numberParts;
    os << ",numberElements=" << d.numberElements;
    os << ",from=";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

void matchInnerType(const PartitionValue& src, PartitionValue& target) {
    mpark::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
        },
        src.members);
}

void matchInnerType(const PartitionDomain& domain, PartitionValue& target) {
    mpark::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<decltype(
                    innerDomainImpl)>::element_type>::type>::type>();
        },
        domain.inner);
}

template <>
UInt getDomainSize<PartitionDomain>(const PartitionDomain&) {
    return MAX_DOMAIN_SIZE;
}

void evaluateImpl(PartitionValue&) {}
void startTriggeringImpl(PartitionValue&) {}
void stopTriggering(PartitionValue&) {}

template <typename InnerViewType>
void normaliseImpl(PartitionValue& partition,
                   ExprRefVec<InnerViewType>& valMembersImpl) {
    for (auto& v : valMembersImpl) {
        normalise(*assumeAsValue(v));
    }
    sort(valMembersImpl.begin(), valMembersImpl.end(),
         [](auto& u, auto& v) { return smallerValue(u->view(), v->view()); });
    for (size_t i = 0; i < valMembersImpl.size(); i++) {
        partition.hashIndexMap[getValueHash(valMembersImpl[i])] = i;
    }
}

template <>
void normalise<PartitionValue>(PartitionValue& val) {
    mpark::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValue<PartitionView>(const PartitionView& u,
                                 const PartitionView& v);
template <>
bool largerValue<PartitionView>(const PartitionView& u, const PartitionView& v);

template <>
bool smallerValue<PartitionView>(const PartitionView&, const PartitionView&) {
    return false;
}

template <>
bool largerValue<PartitionView>(const PartitionView&, const PartitionView&) {
    return false;
}

void PartitionView::standardSanityChecksForThisType() const {
    HashType checkCachedHashTotal = INITIAL_HASH;
    vector<HashType> checkPartHashes(partHashes.size(), INITIAL_HASH);
    mpark::visit(
        [&](auto& members) {
            for (size_t i = 0; i < members.size(); i++) {
                auto& member = members[i];
                auto viewOption = member->getViewIfDefined();
                sanityCheck(viewOption, NO_PARTITION_UNDEFINED);
                auto& view = *viewOption;
                HashType hash = getValueHash(view);
                sanityCheck(
                    hashIndexMap.count(hash),
                    toString("hash ", hash, " missing from hashIndexMap."));
                sanityEqualsCheck(i, hashIndexMap.at(hash));
                UInt part = memberPartMap[i];
                sanityCheck(
                    part < checkPartHashes.size(),
                    toString("Expected ", checkPartHashes.size(),
                             " parts but member ", i, " maps to part ", part));
                checkPartHashes[part] += mix(hash);
            }
            for (size_t i = 0; i < checkPartHashes.size(); i++) {
                sanityEqualsCheck(checkPartHashes[i], partHashes[i]);
                checkCachedHashTotal += mix(checkPartHashes[i]);
            }

            sanityEqualsCheck(checkCachedHashTotal, cachedHashTotal);
            sanityEqualsCheck(members.size(), hashIndexMap.size());
            sanityEqualsCheck(memberPartMap.size(), members.size());
        },
        members);

    vector<bool> foundParts(numberParts(), false);
    for (size_t i = 0; i < memberPartMap.size(); i++) {
        auto part = memberPartMap[i];
        sanityCheck(part < numberParts(),
                    toString("found part ", part, " but number parts is ",
                             numberParts()));
        foundParts[part] = true;
    }
    for (size_t i = 0; i < foundParts.size(); i++) {
        sanityCheck(foundParts[i], toString("Could not find part ", i));
    }
}

void PartitionValue::debugSanityCheckImpl() const {
    mpark::visit(
        [&](auto& members) {
            recurseSanityChecks(members);
            standardSanityChecksForThisType();
            varBaseSanityChecks(*this, members);
        },
        members);
}

template <>
bool hasVariableSize<PartitionValue>(const PartitionValue&) {
    return false;
}
template <>
UInt getSize<PartitionValue>(const PartitionValue&) {
    return 0;
}

AnyExprVec& PartitionValue::getChildrenOperands() { return members; }

template <>
size_t getNumberElementsLowerBound<PartitionDomain>(
    const PartitionDomain& domain) {
    return getDomainSize(domain.inner) *
           getNumberElementsLowerBound(domain.inner);
}
