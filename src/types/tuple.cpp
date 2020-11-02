#include <cassert>
#include <numeric>
#include "common/common.h"
#include "types/tupleVal.h"
#include "utils/ignoreUnused.h"
using namespace std;
static HashType calcHash(const TupleView& val) {
    HashType input[val.members.size()];
    for (size_t i = 0; i < val.members.size(); i++) {
        input[i] = getValueHash(val.members[i]);
    }
    return mix(((char*)input), sizeof(input));
}

template <>
HashType getValueHash<TupleView>(const TupleView& val) {
    return val.cachedHashTotal.getOrSet([&]() { return calcHash(val); });
}

template <>
ostream& prettyPrint<TupleView>(ostream& os, const TupleView& v) {
    os << "(";
    bool first = true;
    for (auto& member : v.members) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        lib::visit([&](auto& member) { prettyPrint(os, member->view()); },
                   member);
    }
    os << ")";
    return os;
}

template <>
ostream& prettyPrint<TupleView>(ostream& os, const TupleDomain& domain,
                                const TupleView& v) {
    os << ((domain.isRecord) ? "record {" : "(");
    for (size_t i = 0; i < v.members.size(); i++) {
        if (i != 0) {
            os << ",";
        }
        if (domain.isRecord) {
            debug_code(assert(i < domain.recordIndexNameMap.size()));
            os << domain.recordIndexNameMap[i] << " = ";
        }

        lib::visit(
            [&](auto& member) {
                typedef
                    typename AssociatedValueType<viewType(member)>::type Value;
                typedef typename AssociatedDomain<Value>::type Domain;
                const auto& domainPtr =
                    lib::get<shared_ptr<Domain>>(domain.inners[i]);
                prettyPrint(os, *domainPtr, member->view());
            },
            v.members[i]);
    }
    os << ((domain.isRecord) ? "}" : ")");
    return os;
}

template <>
void deepCopy<TupleValue>(const TupleValue& src, TupleValue& target) {
    target.silentClear();
    for (auto& member : src.members) {
        lib::visit(
            [&](auto& member) {
                target.addMember(deepCopy(*assumeAsValue(member)));
            },
            member);
    }
    target.notifyEntireValueChanged();
}

template <>
ostream& prettyPrint<TupleDomain>(ostream& os, const TupleDomain& d) {
    os << ((d.isRecord) ? "record {" : "tuple (");
    bool first = true;
    for (size_t i = 0; i < d.inners.size(); i++) {
        auto& innerDomain = d.inners[i];
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        if (d.isRecord) {
            os << d.recordIndexNameMap[i] << " : ";
        }

        prettyPrint(os, innerDomain);
    }
    os << ((d.isRecord) ? "}" : ")");
    return os;
}

void matchInnerType(const TupleValue&, TupleValue&) {}

void matchInnerType(const TupleDomain&, TupleValue&) {}

template <>
UInt getDomainSize<TupleDomain>(const TupleDomain& domain) {
    UInt64 total = 1;
    for (auto& innerDomain : domain.inners) {
        total *= getDomainSize(innerDomain);
    }
    return total;
}

void evaluateImpl(TupleValue&) {}
void startTriggeringImpl(TupleValue&) {}
void stopTriggering(TupleValue&) {}

template <>
void normalise<TupleValue>(TupleValue& val) {
    for (auto& v : val.members) {
        lib::visit([](auto& v) { normalise(*assumeAsValue(v)); }, v);
    }
}

template <>
bool smallerValueImpl<TupleView>(const TupleView& u, const TupleView& v);
template <>
bool largerValueImpl<TupleView>(const TupleView& u, const TupleView& v);
template <>
bool equalValueImpl<TupleView>(const TupleView& u, const TupleView& v);

const AnyValRef toAnyValRef(const AnyExprRef& v) {
    return lib::visit(
        [](const auto& v) -> AnyValRef { return assumeAsValue(v); }, v);
}
template <>
bool smallerValueImpl<TupleView>(const TupleView& u, const TupleView& v) {
    if (u.members.size() < v.members.size()) {
        return true;
    } else if (u.members.size() > v.members.size()) {
        return false;
    }
    for (size_t i = 0; i < u.members.size(); ++i) {
        if (smallerValue(u.members[i], v.members[i])) {
            return true;
        } else if (largerValue(u.members[i], v.members[i])) {
            return false;
        }
    }
    return false;
}

template <>
bool largerValueImpl<TupleView>(const TupleView& u, const TupleView& v) {
    if (u.members.size() > v.members.size()) {
        return true;
    } else if (u.members.size() < v.members.size()) {
        return false;
    }
    for (size_t i = 0; i < u.members.size(); ++i) {
        if (largerValue(u.members[i], v.members[i])) {
            return true;
        } else if (smallerValue(u.members[i], v.members[i])) {
            return false;
        }
    }
    return false;
}

template <>
bool equalValueImpl<TupleView>(const TupleView& u, const TupleView& v) {
    if (u.members.size() != v.members.size()) {
        return false;
    }
    for (size_t i = 0; i < u.members.size(); ++i) {
        if (!equalValue(u.members[i], v.members[i])) {
            return false;
        }
    }
    return true;
}

void TupleValue::assertValidVarBases() {
    bool success = true;
    for (size_t i = 0; i < members.size(); i++) {
        const ValBase& base = valBase(toAnyValRef(members[i]));
        if (base.container != this) {
            success = false;
            myCerr << "member " << i
                   << "'s container does not point to this tuple." << endl;
        } else if (base.id != i) {
            success = false;
            myCerr << "member " << i << "has id " << base.id
                   << " but it should be " << i << endl;
        }
    }
    if (!success) {
        prettyPrint(myCerr, asView(*this)) << endl;
        ;
        this->printVarBases();
        assert(false);
    }
}

void TupleValue::printVarBases() {
    cout << "parent is constant: " << (this->container == &constantPool)
         << endl;
    for (auto& member : members) {
        const ValBase& base = valBase(toAnyValRef(member));
        cout << "val id: " << base.id << endl;
        cout << "is constant: " << (base.container == &constantPool) << endl;
    }
}

void TupleView::standardSanityChecksForThisType() const {
    UInt checkNumberUndefined = 0;
    for (auto& member : members) {
        lib::visit(
            [&](auto& member) {
                if (!member->getViewIfDefined().hasValue()) {
                    ++checkNumberUndefined;
                }
            },
            member);
    }
    sanityEqualsCheck(checkNumberUndefined, numberUndefined);
    if (numberUndefined == 0) {
        sanityCheck(this->appearsDefined(), "operator should be defined.");
    } else {
        sanityCheck(!this->appearsDefined(), "operator should be undefined.");
    }
    cachedHashTotal.applyIfValid(
        [&](const auto& value) { sanityEqualsCheck(calcHash(*this), value); });
}

void TupleValue::debugSanityCheckImpl() const {
    for (auto& member : members) {
        lib::visit(
            [&](const auto& member) {
                member->debugSanityCheck();
                const ValBase& base = valBase(*assumeAsValue(member));
                sanityCheck(base.container == this,
                            toString("Member with id ", base.id,
                                     " does not point to parent."));
            },
            member);
    }
    standardSanityChecksForThisType();
}

template <>
bool hasVariableSize<TupleValue>(const TupleValue&) {
    return false;
}
template <>
UInt getSize<TupleValue>(const TupleValue& v) {
    return v.members.size();
}

template <>
size_t getResourceLowerBound<TupleDomain>(const TupleDomain& domain) {
    return std::accumulate(
        domain.inners.begin(), domain.inners.end(), size_t(0),
        [](size_t a, auto& d) { return a + getResourceLowerBound(d); });
}
