#include <algorithm>
#include <cassert>
#include <cmath>

#include "common/common.h"
#include "types/sequenceVal.h"
#include "utils/ignoreUnused.h"
#include "utils/safePow.h"
using namespace std;
size_t numberElements(SequenceView& view) { return view.numberElements(); }
template <>
HashType getValueHash<SequenceView>(const SequenceView& val) {
    return val.cachedHashTotal.getOrSet([&]() {
        return lib::visit(
            [&](auto& members) {
                return val.calcSubsequenceHash<viewType(members)>(
                    0, members.size());
            },
            val.members);
    });
}

template <>
ostream& prettyPrint<SequenceView>(ostream& os, const SequenceView& v) {
    os << "sequence(";
    lib::visit(
        [&](auto& membersImpl) {
            bool first = true;
            for (auto& memberPtr : membersImpl) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                prettyPrint(os, memberPtr->getViewIfDefined());
            }
        },
        v.members);
    os << ")";
    return os;
}

template <>
ostream& prettyPrint<SequenceView>(ostream& os, const SequenceDomain& domain,
                                   const SequenceView& v) {
    os << "sequence(";
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
                prettyPrint(os, *domainPtr, memberPtr->getViewIfDefined());
            }
        },
        v.members);
    os << ")";
    return os;
}

template <typename InnerViewType>
void deepCopyImpl(const SequenceValue&,
                  const ExprRefVec<InnerViewType>& srcMemnersImpl,
                  SequenceValue& target) {
    target.cachedHashTotal.invalidate();
    // to be optimised later
    target.silentClear();
    for (auto& member : srcMemnersImpl) {
        target.addMember(target.numberElements(),
                         deepCopy(*assumeAsValue(member)));
    }
    debug_code(target.assertValidState());
    target.notifyEntireValueChanged();
}

template <>
void deepCopy<SequenceValue>(const SequenceValue& src, SequenceValue& target) {
    assert(src.members.index() == target.members.index());
    return visit(
        [&](auto& srcMembersImpl) {
            return deepCopyImpl(src, srcMembersImpl, target);
        },
        src.members);
}

template <>
ostream& prettyPrint<SequenceDomain>(ostream& os, const SequenceDomain& d) {
    os << "sequence(";
    os << d.sizeAttr << ",";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

void matchInnerType(const SequenceValue& src, SequenceValue& target) {
    lib::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
            target.injective = src.injective;
        },
        src.members);
}

void matchInnerType(const SequenceDomain& domain, SequenceValue& target) {
    lib::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<
                    decltype(innerDomainImpl)>::element_type>::type>::type>();
            target.injective = domain.injective;
        },
        domain.inner);
}

template <>
UInt getDomainSize<SequenceDomain>(const SequenceDomain& domain) {
    UInt innerDomainSize = getDomainSize(domain.inner);
    UInt domainSize = 0;
    for (UInt i = domain.sizeAttr.minSize; i <= domain.sizeAttr.maxSize; i++) {
        auto crossProd = safePow<UInt>(innerDomainSize, i, MAX_DOMAIN_SIZE);
        if (!crossProd || MAX_DOMAIN_SIZE - domainSize < crossProd) {
            domainSize = MAX_DOMAIN_SIZE;
            break;
        }
        domainSize += *crossProd;
    }
    return domainSize;
}

void evaluateImpl(SequenceValue&) {}
void startTriggeringImpl(SequenceValue&) {}
void stopTriggering(SequenceValue&) {}

template <typename InnerViewType>
void normaliseImpl(SequenceValue&, ExprRefVec<InnerViewType>& valMembersImpl) {
    for (auto& v : valMembersImpl) {
        normalise(*assumeAsValue(v));
    }
}

template <>
void normalise<SequenceValue>(SequenceValue& val) {
    lib::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValueImpl<SequenceView>(const SequenceView& u,
                                    const SequenceView& v);
template <>
bool largerValueImpl<SequenceView>(const SequenceView& u,
                                   const SequenceView& v);
template <>
bool equalValueImpl<SequenceView>(const SequenceView& u, const SequenceView& v);

template <>
bool smallerValueImpl<SequenceView>(const SequenceView& u,
                                    const SequenceView& v) {
    return lib::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                lib::get<BaseType<decltype(uMembersImpl)>>(v.members);
            if (uMembersImpl.size() < vMembersImpl.size()) {
                return true;
            } else if (uMembersImpl.size() > vMembersImpl.size()) {
                return false;
            }
            for (size_t i = 0; i < uMembersImpl.size(); ++i) {
                if (smallerValue(uMembersImpl[i]->getViewIfDefined(),
                                 vMembersImpl[i]->getViewIfDefined())) {
                    return true;
                } else if (largerValue(uMembersImpl[i]->getViewIfDefined(),
                                       vMembersImpl[i]->getViewIfDefined())) {
                    return false;
                }
            }
            return false;
        },
        u.members);
}

template <>
bool largerValueImpl<SequenceView>(const SequenceView& u,
                                   const SequenceView& v) {
    return lib::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                lib::get<BaseType<decltype(uMembersImpl)>>(v.members);
            if (uMembersImpl.size() > vMembersImpl.size()) {
                return true;
            } else if (uMembersImpl.size() < vMembersImpl.size()) {
                return false;
            }
            for (size_t i = 0; i < uMembersImpl.size(); ++i) {
                if (largerValue(uMembersImpl[i]->getViewIfDefined(),
                                vMembersImpl[i]->getViewIfDefined())) {
                    return true;
                } else if (smallerValue(uMembersImpl[i]->getViewIfDefined(),
                                        vMembersImpl[i]->getViewIfDefined())) {
                    return false;
                }
            }
            return false;
        },
        u.members);
}

template <>
bool equalValueImpl<SequenceView>(const SequenceView& u,
                                  const SequenceView& v) {
    return lib::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                lib::get<BaseType<decltype(uMembersImpl)>>(v.members);
            if (uMembersImpl.size() != vMembersImpl.size()) {
                return false;
            }
            for (size_t i = 0; i < uMembersImpl.size(); ++i) {
                if (!equalValue(uMembersImpl[i]->getViewIfDefined(),
                                vMembersImpl[i]->getViewIfDefined())) {
                    return false;
                }
            }
            return true;
        },
        u.members);
}

void SequenceView::assertValidState() {
    lib::visit(
        [&](auto& valMembersImpl) {
            UInt numberUndefinedFound = 0;
            bool success = true;
            HashType calculatedTotal(0);
            for (size_t i = 0; i < valMembersImpl.size(); i++) {
                auto& member = valMembersImpl[i];
                if (cachedHashTotal.isValid()) {
                    auto view = member->getViewIfDefined();
                    if (view) {
                        calculatedTotal += this->calcMemberHash(i, member);
                    }
                }
                if (!member->appearsDefined()) {
                    numberUndefinedFound++;
                }
            }
            if (success) {
                cachedHashTotal.applyIfValid([&](const auto& value) {
                    success = (value == calculatedTotal);
                    if (!success) {
                        myCerr << "Calculated hash total should be "
                               << calculatedTotal << " but it was actually "
                               << value << endl;
                        ;
                    }
                });
            }
            if (success) {
                success = numberUndefined == numberUndefinedFound;
                if (!success) {
                    myCerr << "Error: numberUndefined = " << numberUndefined
                           << " but think it should be " << numberUndefinedFound
                           << endl;
                    myCerr << "members size " << valMembersImpl.size() << endl;
                    success = false;
                }
            }
            if (!success) {
                myCerr << "sequence Members: " << valMembersImpl << endl;
                assert(false);
                myAbort();
            }
        },
        members);
}

void SequenceValue::assertValidState() {
    lib::visit(
        [&](auto& valMembersImpl) {
            bool success = true;
            if (injective) {
                for (size_t i = 0; i < valMembersImpl.size(); i++) {
                    auto& member = valMembersImpl[i];
                    HashType hash =
                        getValueHash(member->getViewIfDefined().get());
                    if (!memberHashes.count(hash)) {
                        myCerr << "Error: member " << member->getViewIfDefined()
                               << " with hash " << hash
                               << " is not in memberHashes.\n";
                        success = false;
                        break;
                    }
                }
                if (success) {
                    if (memberHashes.size() != numberElements()) {
                        myCerr << "Found member hashes of elements not in the "
                                  "sequence.\n";
                        success = false;
                    }
                }
            }
            if (!success) {
                myCerr << "sequence Members: " << valMembersImpl << endl;
                if (injective) {
                    myCerr << "memberHashes: " << memberHashes << endl;
                }
                assert(false);
                myAbort();
            }
        },
        members);
}

void SequenceView::standardSanityChecksForThisType() const {
    lib::visit(
        [&](auto& members) {
            UInt checkNumberUndefined = 0;
            for (auto& member : members) {
                if (!member->getViewIfDefined().hasValue()) {
                    ++checkNumberUndefined;
                }
            }
            sanityEqualsCheck(checkNumberUndefined, numberUndefined);
            if (numberUndefined == 0) {
                sanityCheck(this->appearsDefined(),
                            "operator should be defined.");
            } else {
                sanityCheck(!this->appearsDefined(),
                            "operator should be undefined.");
            }
            cachedHashTotal.applyIfValid([&](const auto& value) {
                sanityEqualsCheck(
                    calcSubsequenceHash<viewType(members)>(0, members.size()),
                    value);
            });
        },
        this->members);
}

void SequenceValue::debugSanityCheckImpl() const {
    lib::visit(
        [&](const auto& valMembersImpl) {
            recurseSanityChecks(valMembersImpl);
            standardSanityChecksForThisType();
            if (injective) {
                for (size_t i = 0; i < valMembersImpl.size(); i++) {
                    auto& member = valMembersImpl[i];
                    auto viewOption = member->getViewIfDefined();
                    sanityCheck(
                        viewOption,
                        "all members must be defined in sequence values.");
                    auto& view = *viewOption;
                    HashType hash = getValueHash(view);
                    sanityCheck(
                        memberHashes.count(hash),
                        toString("member ", member->getViewIfDefined(),
                                 " with hash ", hash,
                                 " is not in memberHashes.\nMember hashes is ",
                                 memberHashes));
                }
                sanityEqualsCheck(memberHashes.size(), numberElements());
            }
            // check var bases

            varBaseSanityChecks(*this, valMembersImpl);
        },
        members);
}

void SequenceValue::assertValidVarBases() {
    lib::visit(
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
                    myCerr << "member " << i
                           << "'s container does not point to this sequence."
                           << endl;
                } else if (base.id != i) {
                    success = false;
                    myCerr << "sequence member " << i << "has id " << base.id
                           << " but it should be " << i << endl;
                }
            }
            if (!success) {
                myCerr << "Members: " << valMembersImpl << endl;
                this->printVarBases();
                assert(false);
            }
        },
        members);
}

void SequenceValue::printVarBases() {
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

template <>
bool hasVariableSize<SequenceValue>(const SequenceValue&) {
    return true;
}
template <>
UInt getSize<SequenceValue>(const SequenceValue& v) {
    return v.numberElements();
}
AnyExprVec& SequenceValue::getChildrenOperands() { return members; }

template <>
size_t getResourceLowerBound<SequenceDomain>(const SequenceDomain& domain) {
    return domain.sizeAttr.minSize * getResourceLowerBound(domain.inner) + 1;
}
