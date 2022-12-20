#include <algorithm>
#include <cassert>

#include "common/common.h"
#include "types/partitionVal.h"
#include "utils/ignoreUnused.h"
using namespace std;

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
    lib::visit(
        [&](auto& membersImpl) {
            vector<ExprRefVec<viewType(membersImpl)>> parts(v.numberElements());
            for (size_t i = 0; i < v.memberPartMap.size(); i++) {
                parts[v.memberPartMap[i]].emplace_back(membersImpl[i]);
            }
            bool first = true;
            for (auto& part : parts) {
                if (part.empty()) {
                    continue;
                }
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
    os << "\n$memberPartMap=" << v.memberPartMap << "\n";
    return os;
}

template <>
ostream& prettyPrint<PartitionView>(ostream& os, const PartitionDomain& domain,
                                    const PartitionView& v) {
    os << "partition(";
    lib::visit(
        [&](auto& membersImpl) {
            typedef
                typename AssociatedValueType<viewType(membersImpl)>::type Value;
            typedef typename AssociatedDomain<Value>::type Domain;
            const auto& domainPtr = lib::get<shared_ptr<Domain>>(domain.inner);

            vector<ExprRefVec<viewType(membersImpl)>> parts(v.numberElements());
            for (size_t i = 0; i < v.memberPartMap.size(); i++) {
                parts[v.memberPartMap[i]].emplace_back(membersImpl[i]);
            }
            bool first = true;
            for (auto& part : parts) {
                if (part.empty()) {
                    continue;
                }
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
    os << "\n$number parts = " << v.numberParts() << "\n";

    return os;
}

template <typename InnerViewType>
void deepCopyImpl(const PartitionValue& src,
                  const ExprRefVec<InnerViewType>& srcMembersImpl,
                  PartitionValue& target) {
    typedef typename AssociatedValueType<InnerViewType>::type InnerValueType;

    target.silentClear();
    target.setNumberElements<InnerValueType>(srcMembersImpl.size());

    for (size_t i = 0; i < srcMembersImpl.size(); i++) {
        target.assignMember(i, src.memberPartMap[i],
                            deepCopy(*assumeAsValue(srcMembersImpl[i])));
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
    os << ", partSize=" << d.partSize;
    os << ",numberElements=" << d.numberElements;
    os << ",from=";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

void matchInnerType(const PartitionValue& src, PartitionValue& target) {
    lib::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
        },
        src.members);
}

void matchInnerType(const PartitionDomain& domain, PartitionValue& target) {
    lib::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<
                    decltype(innerDomainImpl)>::element_type>::type>::type>();
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
    todoImpl(partition, valMembersImpl);
}

template <>
void normalise<PartitionValue>(PartitionValue& val) {
    lib::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValueImpl<PartitionView>(const PartitionView& u,
                                     const PartitionView& v);
template <>
bool largerValueImpl<PartitionView>(const PartitionView& u,
                                    const PartitionView& v);
template <>
bool equalValueImpl<PartitionView>(const PartitionView& u,
                                   const PartitionView& v);

template <>
bool smallerValueImpl<PartitionView>(const PartitionView& u,
                                     const PartitionView& v) {
    todoImpl(u, v);
}

template <>
bool largerValueImpl<PartitionView>(const PartitionView& u,
                                    const PartitionView& v) {
    todoImpl(u, v);
}

template <>
bool equalValueImpl<PartitionView>(const PartitionView& u,
                                   const PartitionView& v) {
    todoImpl(u, v);
}

void PartitionView::standardSanityChecksForThisType() const {
    lib::visit(
        [&](auto& members) {
            HashType checkCachedHashTotal = HashType(0);
            vector<PartInfo> checkPartInfo(partInfo.size());
            sanityEqualsCheck(members.size(), partInfo.size());
            sanityEqualsCheck(members.size(), memberPartMap.size());
            sanityEqualsCheck(members.size(), hashIndexMap.size());

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
                checkPartInfo[part].notifyMemberAdded(member);
            }
            for (size_t i = 0; i < checkPartInfo.size(); i++) {
                sanityEqualsCheck(checkPartInfo[i], partInfo[i]);
                checkCachedHashTotal += checkPartInfo[i].mixedPartHash();
            }

            sanityEqualsCheck(checkCachedHashTotal, cachedHashTotal);
        },
        members);

    size_t checkNumberParts = 0;
    for (size_t i = 0; i < partInfo.size(); i++) {
        if (partInfo[i].partSize > 0) {
            sanityCheck(parts.count(i), toString(i, " missing from parts set"));
            sanityCheck(!emptyParts.count(i),
                        toString(i, " should not be in emptyParts set"));
            checkNumberParts += 1;
        }
    }
    sanityEqualsCheck(checkNumberParts, parts.size());
    sanityEqualsCheck(checkNumberParts + emptyParts.size(), partInfo.size());
}

void PartitionValue::debugSanityCheckImpl() const {
    lib::visit(
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
size_t getResourceLowerBound<PartitionDomain>(const PartitionDomain& domain) {
    return getDomainSize(domain.inner) * getResourceLowerBound(domain.inner);
}
