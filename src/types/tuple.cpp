#include "types/tuple.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <>
HashType getValueHash<TupleView>(const TupleView& val) {
    return val.cachedHashTotal.getOrSet([&]() {
        HashType result[2];
        HashType input[val.members.size()];
        for (size_t i = 0; i < val.members.size(); i++) {
            input[i] =
                mpark::visit([&](auto& expr) { return getValueHash(*expr); },
                             val.members[i]);
        }
        MurmurHash3_x64_128(((void*)input), sizeof(input), 0, result);
        return result[0] ^ result[1];
    });
}

template <>
ostream& prettyPrint<TupleView>(ostream& os, const TupleView& v) {
    os << "Tuple(";
    bool first = true;
    for (auto& member : v.members) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        mpark::visit([&](auto& member) { prettyPrint(os, *member); }, member);
    }
    os << ")";
    return os;
}

template <>
void deepCopy<TupleValue>(const TupleValue& src, TupleValue& target) {
    target.notifyPossibleEntireTupleChange();
    target.members.clear();
    for (auto& member : src.members) {
        mpark::visit(
            [&](auto& expr) {
                target.addMember(deepCopy(assumeAsValue(*expr)));
            },
            member);
    }
    target.entireTupleChangeAndNotify();
}

template <>
ostream& prettyPrint<TupleDomain>(ostream& os, const TupleDomain& d) {
    os << "tuple(";
    bool first = true;
    for (auto& innerDomain : d.inners) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        prettyPrint(os, innerDomain);
    }
    os << ")";
    return os;
}

void matchInnerType(const TupleValue&, TupleValue&) {}

void matchInnerType(const TupleDomain&, TupleValue&) {}

template <>
UInt getDomainSize<TupleDomain>(const TupleDomain& domain) {
    u_int64_t total = 1;
    for (auto& innerDomain : domain.inners) {
        total *= getDomainSize(innerDomain);
    }
    return total;
}

void evaluate(TupleValue&) {}
void startTriggering(TupleValue&) {}
void stopTriggering(TupleValue&) {}

template <>
void normalise<TupleValue>(TupleValue& val) {
    for (auto& v : val.members) {
        mpark::visit([](auto& v) { normalise(assumeAsValue(*v)); }, v);
    }
}

template <>
bool smallerValue<TupleValue>(const TupleValue& u, const TupleValue& v);
template <>
bool largerValue<TupleValue>(const TupleValue& u, const TupleValue& v);
const AnyValRef toAnyValRef(const AnyExprRef& v) {
    return mpark::visit(
        [](const auto& v) -> AnyValRef { return assumeAsValue(v.asViewRef()); },
        v);
}
template <>
bool smallerValue<TupleValue>(const TupleValue& u, const TupleValue& v) {
    if (u.members.size() < v.members.size()) {
        return true;
    } else if (u.members.size() > v.members.size()) {
        return false;
    }
    for (size_t i = 0; i < u.members.size(); ++i) {
        if (smallerValue(toAnyValRef(u.members[i]),
                         toAnyValRef(v.members[i]))) {
            return true;
        } else if (largerValue(toAnyValRef(u.members[i]),
                               toAnyValRef(v.members[i]))) {
            return false;
        }
    }
    return false;
}

template <>
bool largerValue<TupleValue>(const TupleValue& u, const TupleValue& v) {
    if (u.members.size() > v.members.size()) {
        return true;
    } else if (u.members.size() < v.members.size()) {
        return false;
    }
    for (size_t i = 0; i < u.members.size(); ++i) {
        if (largerValue(toAnyValRef(u.members[i]), toAnyValRef(v.members[i]))) {
            return true;
        } else if (smallerValue(toAnyValRef(u.members[i]),
                                toAnyValRef(v.members[i]))) {
            return false;
        }
    }
    return false;
}

void TupleValue::assertValidVarBases() {
    bool success = true;
    for (size_t i = 0; i < members.size(); i++) {
        const ValBase& base = valBase(toAnyValRef(members[i]));
        if (base.container != this) {
            success = false;
            cerr << "member " << i
                 << "'s container does not point to this tuple." << endl;
        } else if (base.id != i) {
            success = false;
            cerr << "member " << i << "has id " << base.id
                 << " but it should be " << i << endl;
        }
    }
    if (!success) {
        prettyPrint(cerr, asView(*this)) << endl;
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