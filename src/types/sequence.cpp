#include "types/sequence.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include "common/common.h"
#include "utils/ignoreUnused.h"
#include "utils/safePow.h"
using namespace std;
size_t numberUndefinedMembers(const SequenceView& view) {
    return view.numberUndefined;
}

template <>
HashType getValueHash<SequenceView>(const SequenceView& val) {
    return val.cachedHashTotal.getOrSet([&]() {
        return mpark::visit(
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
void deepCopyImpl(const SequenceValue&,
                  const ExprRefVec<InnerViewType>& srcMemnersImpl,
                  SequenceValue& target) {
    // to be optimised later
    target.silentClear();
    for (auto& member : srcMemnersImpl) {
        target.addMember(target.numberElements(),
                         deepCopy(*assumeAsValue(member)));
    }
    debug_code(target.assertValidState());
    target.notifyEntireSequenceChange();
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
    mpark::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
            target.injective = src.injective;
        },
        src.members);
}

void matchInnerType(const SequenceDomain& domain, SequenceValue& target) {
    mpark::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<decltype(
                    innerDomainImpl)>::element_type>::type>::type>();
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
        domainSize += crossProd;
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
    mpark::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.members);
}

template <>
bool smallerValue<SequenceView>(const SequenceView& u, const SequenceView& v);
template <>
bool largerValue<SequenceView>(const SequenceView& u, const SequenceView& v);

template <>
bool smallerValue<SequenceView>(const SequenceView& u, const SequenceView& v) {
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
bool largerValue<SequenceView>(const SequenceView& u, const SequenceView& v) {
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

void SequenceView::assertValidState() {
    mpark::visit(
        [&](auto& valMembersImpl) {
            UInt numberUndefinedFound = 0;
            bool success = true;
            UInt calculatedTotal = 0;
            for (size_t i = 0; i < valMembersImpl.size(); i++) {
                auto& member = valMembersImpl[i];
                if (cachedHashTotal.isValid() && !member->isUndefined()) {
                    calculatedTotal += this->calcMemberHash(i, member);
                }
                if (member->isUndefined()) {
                    numberUndefinedFound++;
                }
            }
            if (success) {
                cachedHashTotal.applyIfValid([&](const auto& value) {
                    success = (value == calculatedTotal);
                    if (!success) {
                        cerr << "Calculated hash total should be "
                             << calculatedTotal << " but it was actually "
                             << value << endl;
                        ;
                    }
                });
            }
            if (success) {
                success = numberUndefined == numberUndefinedFound;
                if (!success) {
                    cerr << "Error: numberUndefined = " << numberUndefined
                         << " but think it should be " << numberUndefinedFound
                         << endl;
                    cerr << "members size " << valMembersImpl.size() << endl;
                }
            }
            if (!success) {
                cerr << "sequence Members: " << valMembersImpl << endl;
                assert(false);
                abort();
            }
        },
        members);
}

void SequenceValue::assertValidState() {
    mpark::visit(
        [&](auto& valMembersImpl) {
            bool success = true;
            if (injective) {
                for (size_t i = 0; i < valMembersImpl.size(); i++) {
                    auto& member = valMembersImpl[i];
                    HashType hash = getValueHash(member->view());
                    if (!memberHashes.count(hash)) {
                        cerr << "Error: member " << member->view()
                             << " with hash " << hash
                             << " is not in memberHashes.\n";
                        success = false;
                        break;
                    }
                }
                if (success) {
                    if (memberHashes.size() != numberElements()) {
                        cerr << "Found member hashes of elements not in the "
                                "sequence.\n";
                        success = false;
                    }
                }
            }
            if (!success) {
                cerr << "sequence Members: " << valMembersImpl << endl;
                if (injective) {
                    cerr << "memberHashes: " << memberHashes << endl;
                }
                assert(false);
                abort();
            }
        },
        members);
}

void SequenceValue::assertValidVarBases() {
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
                         << "'s container does not point to this sequence."
                         << endl;
                } else if (base.id != i) {
                    success = false;
                    cerr << "sequence member " << i << "has id " << base.id
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

void SequenceValue::printVarBases() {
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
